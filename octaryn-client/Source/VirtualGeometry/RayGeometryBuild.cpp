#include "RayGeometryInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <slang-rhi/acceleration-structure-utils.h>
#include <algorithm>
#include <set>
namespace octaryn::client::rendering::virtual_geometry {
using namespace ray_geometry;
namespace {
const auto storage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
const auto geometry=storage|rhi::BufferUsage::AccelerationStructureBuildInput;
void bind(rhi::IShaderObject* root,const char* name,rhi::IBuffer* buffer) {checked(rhi::ShaderCursor(root)[name].setBinding(rhi::Binding(buffer)),name);}
rhi::AccelerationStructureBuildDesc build_desc(const rhi::AccelerationStructureBuildInput* inputs,std::uint32_t count,bool compact) {
  rhi::AccelerationStructureBuildDesc desc{};desc.inputs=inputs;desc.inputCount=count;desc.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  if(compact)desc.flags|=rhi::AccelerationStructureBuildFlags::AllowCompaction;return desc;
}
}
void RayGeometry::State::tlas(Build& pending,rhi::ICommandEncoder* commands) {
  auto& scene=*pending.scene;std::vector<rhi::AccelerationStructureInstanceDescGeneric> generic(scene.blas.size());
  for(std::size_t i=0;i<generic.size();++i) {
    auto& instance=generic[i];instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
    instance.instanceID=static_cast<std::uint32_t>(i);instance.instanceMask=255;instance.accelerationStructure=scene.blas[i]->getHandle();
    instance.flags=rhi::AccelerationStructureInstanceFlags::TriangleFacingCullDisable;
  }
  const auto type=rhi::getAccelerationStructureInstanceDescType(device);const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  check(stride>0,"ray instance layout unavailable");std::vector<std::uint8_t> native(generic.size()*stride);
  rhi::convertAccelerationStructureInstanceDescs(generic.size(),type,native.data(),stride,generic.data(),sizeof(generic[0]));
  pending.instances=buffer(device,native.size(),static_cast<std::uint32_t>(stride),rhi::BufferUsage::AccelerationStructureBuildInput);
  rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
  input.instances.instanceBuffer=pending.instances;input.instances.instanceStride=static_cast<std::uint32_t>(stride);input.instances.instanceCount=static_cast<std::uint32_t>(generic.size());
  const auto desc=build_desc(&input,1,false);rhi::AccelerationStructureSizes sizes{};checked(device->getAccelerationStructureSizes(desc,&sizes),"ray TLAS sizes failed");
  const auto extra=sizes.accelerationStructureSize+std::max<std::uint64_t>(sizes.scratchSize,4)+native.size();
  check(pending.build_bytes<=config.maximum_build_bytes&&extra<=config.maximum_build_bytes-pending.build_bytes,"ray TLAS build budget exceeded");
  const auto used=live_bytes()+pending.build_bytes;
  check(used<=config.maximum_resident_bytes&&extra<=config.maximum_resident_bytes-used,"ray TLAS resident budget exceeded");
  pending.build_bytes+=extra;
  scene.tlas=acceleration(device,rhi::AccelerationStructureKind::TopLevel,sizes.accelerationStructureSize);scene.bytes+=sizes.accelerationStructureSize;
  pending.scratch=buffer(device,std::max<std::uint64_t>(sizes.scratchSize,4),4,rhi::BufferUsage::UnorderedAccess);
  checked(commands->uploadBufferData(pending.instances,0,native.size(),native.data()),"ray instances upload failed");commands->globalBarrier();
  commands->buildAccelerationStructure(desc,scene.tlas,nullptr,pending.scratch,0,nullptr);commands->globalBarrier();
}
bool RayGeometry::record(rhi::ICommandEncoder* commands,rhi::IBuffer* pool,std::span<const GpuPage> pages,const SelectionView& view) {
  auto& s=*state_;try {
    check(s.device&&commands&&pool,"ray geometry is not initialized");check(!s.pending,"ray replacement already pending");
    check(pool->getDesc().size<=UINT32_MAX,"ray page pool exceeds 32-bit byte addressing");
    SelectionView ray_view=view;ray_view.frustum=false;ray_view.error_pixels=s.config.error_pixels;SelectionResult selected;
    const bool selected_ok=select_geometry(s.topology,pages,ray_view,s.config.maximum_clusters,s.config.feedback_capacity,selected,s.error);s.feedback=std::move(selected.requests);
    if(!selected_ok)throw std::runtime_error(s.error);check(!selected.clusters.empty(),"empty ray coverage cut");
    auto next=std::make_unique<State::Build>();next->scene=std::make_shared<RaySnapshot>();auto& scene=*next->scene;
    scene.source_hash=s.asset.source_hash;scene.clusters=spatial_order(s.asset,std::move(selected.clusters));
    std::vector<RayCopyCluster> copies;std::uint32_t vertex_count=0,triangle_count=0;std::set<std::uint32_t> source_pages;
    for(std::size_t i=0;i<scene.clusters.size();++i) {
      const auto id=scene.clusters[i];const auto& c=s.asset.clusters[id];check(c.page<pages.size(),"ray page table range");const auto& page=pages[c.page];
      check(page.resident&&page.generation&&page.slot!=invalid_page,"ray source page missing");
      check((std::uint64_t(page.slot)+1)*page_bytes<=pool->getDesc().size,"ray source pool range");
      if(source_pages.insert(c.page).second)next->source_pages.push_back({page.slot,page.generation});
      if(i%s.config.clusters_per_blas==0)scene.batches.push_back({triangle_count,0,static_cast<std::uint32_t>(i),0});
      auto& batch=scene.batches.back();batch.triangle_count+=c.triangle_count;++batch.cluster_count;
      copies.push_back({id,c.page,page.slot,page.generation,c.vertex_offset,c.triangle_offset,c.vertex_count,c.triangle_count,vertex_count,triangle_count,c.material,c.flags});
      vertex_count+=c.vertex_count;triangle_count+=c.triangle_count;
    }
    const auto vertex_bytes=std::uint64_t(vertex_count)*sizeof(MapVertex),index_bytes=std::uint64_t(triangle_count)*12,record_bytes=std::uint64_t(triangle_count)*sizeof(RayTriangle);
    scene.bytes=vertex_bytes+index_bytes+record_bytes+scene.batches.size()*sizeof(RayBatch);
    std::uint64_t build_bytes=scene.bytes+copies.size()*sizeof(RayCopyCluster)+pages.size()*sizeof(GpuPage)+4;
    check(build_bytes<=s.config.maximum_build_bytes&&s.live_bytes()+build_bytes<=s.config.maximum_resident_bytes,"ray expansion budget exceeded");
    scene.vertices=buffer(s.device,vertex_bytes,0,geometry);scene.indices=buffer(s.device,index_bytes,4,geometry);
    scene.triangle_records=buffer(s.device,record_bytes,sizeof(RayTriangle),storage);scene.batch_records=buffer(s.device,scene.batches.size()*sizeof(RayBatch),sizeof(RayBatch),storage,scene.batches.data());
    next->copies=buffer(s.device,copies.size()*sizeof(RayCopyCluster),sizeof(RayCopyCluster),storage,copies.data());
    next->pages=buffer(s.device,pages.size()*sizeof(GpuPage),sizeof(GpuPage),storage,pages.data());const std::uint32_t zero=0;next->validation=buffer(s.device,4,4,storage,&zero);
    for(const auto& batch:scene.batches) {
      rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Triangles;auto& triangles=input.triangles;
      triangles.vertexBuffers[0]=scene.vertices;triangles.vertexBufferCount=1;triangles.vertexFormat=rhi::Format::RGB32Float;triangles.vertexCount=vertex_count;triangles.vertexStride=sizeof(MapVertex);
      triangles.indexBuffer={scene.indices.get(),std::uint64_t(batch.first_triangle)*12};triangles.indexCount=batch.triangle_count*3;triangles.indexFormat=rhi::IndexFormat::Uint32;
      triangles.flags=rhi::AccelerationStructureGeometryFlags::None;
      const auto desc=build_desc(&input,1,true);rhi::AccelerationStructureSizes sizes{};checked(s.device->getAccelerationStructureSizes(desc,&sizes),"ray BLAS sizes failed");
      const auto extra=sizes.accelerationStructureSize+std::max<std::uint64_t>(sizes.scratchSize,4);
      check(build_bytes<=s.config.maximum_build_bytes&&extra<=s.config.maximum_build_bytes-build_bytes,"ray BLAS build budget exceeded");
      build_bytes+=extra;check(s.live_bytes()+build_bytes<=s.config.maximum_resident_bytes,"ray BLAS resident budget exceeded");
      scene.blas.push_back(acceleration(s.device,rhi::AccelerationStructureKind::BottomLevel,sizes.accelerationStructureSize));scene.bytes+=sizes.accelerationStructureSize;
      next->blas_scratch.push_back(buffer(s.device,std::max<std::uint64_t>(sizes.scratchSize,4),4,rhi::BufferUsage::UnorderedAccess));next->blas_inputs.push_back(input);
    }
    rhi::QueryPoolDesc query{};query.type=rhi::QueryType::AccelerationStructureCompactedSize;query.count=static_cast<std::uint32_t>(scene.blas.size());
    checked(s.device->createQueryPool(query,scene.compact_sizes.writeRef()),"ray compaction query creation failed");
    next->build_bytes=build_bytes;s.pending=std::move(next);auto& pending=*s.pending;
    auto pass=commands->beginComputePass();auto root=pass->bindPipeline(s.expand);check(root,"ray expand binding failed");
    bind(root,"copies",pending.copies);bind(root,"pageTable",pending.pages);bind(root,"pagePool",pool);bind(root,"rayVertices",scene.vertices);bind(root,"rayIndices",scene.indices);bind(root,"triangleRecords",scene.triangle_records);bind(root,"validation",pending.validation);
    const auto count=static_cast<std::uint32_t>(copies.size());checked(rhi::ShaderCursor(root)["copyCount"].setData(count),"ray copy count binding");pass->dispatchCompute(count,1,1);pass->end();commands->globalBarrier();
    commands->setBufferState(scene.vertices,rhi::ResourceState::AccelerationStructureBuildInput);commands->setBufferState(scene.indices,rhi::ResourceState::AccelerationStructureBuildInput);
    for(std::uint32_t i=0;i<scene.blas.size();++i) {
      const auto desc=build_desc(&pending.blas_inputs[i],1,true);rhi::AccelerationStructureQueryDesc result{rhi::QueryType::AccelerationStructureCompactedSize,scene.compact_sizes.get(),i};
      commands->buildAccelerationStructure(desc,scene.blas[i],nullptr,pending.blas_scratch[i],1,&result);
    }
    commands->globalBarrier();s.tlas(pending,commands);commands->setBufferState(scene.vertices,rhi::ResourceState::ShaderResource);commands->setBufferState(scene.indices,rhi::ResourceState::ShaderResource);
    pending.recorded=true;s.error.clear();return true;
  } catch(const std::exception& e) {s.error=e.what();return false;}
}
}
