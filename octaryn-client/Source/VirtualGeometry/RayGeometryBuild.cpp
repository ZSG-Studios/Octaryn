#include "RayGeometryInternal.h"
#include "InstanceSelection.h"
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
  if(!config.build_local_tlas)return;
  auto& scene=*pending.scene;std::vector<rhi::AccelerationStructureInstanceDescGeneric> generic(scene.blas.size());
  for(std::size_t i=0;i<generic.size();++i) {
    auto& instance=generic[i];instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
    instance.instanceID=static_cast<std::uint32_t>(i);instance.instanceMask=255;instance.accelerationStructure=scene.blas[i]->getHandle();
    instance.flags=rhi::AccelerationStructureInstanceFlags::TriangleFacingCullDisable;
  }
  const auto type=rhi::getAccelerationStructureInstanceDescType(device);const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  check(stride>0,"ray instance layout unavailable");std::vector<std::uint8_t> native(generic.size()*stride);
  rhi::convertAccelerationStructureInstanceDescs(generic.size(),type,native.data(),stride,generic.data(),sizeof(generic[0]));
  rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
  input.instances.instanceBuffer=scene.vertices;input.instances.instanceStride=static_cast<std::uint32_t>(stride);input.instances.instanceCount=static_cast<std::uint32_t>(generic.size());
  const auto desc=build_desc(&input,1,false);rhi::AccelerationStructureSizes sizes{};checked(device->getAccelerationStructureSizes(desc,&sizes),"ray TLAS sizes failed");
  const auto scratch_size=std::max<std::uint64_t>(sizes.scratchSize,4);
  const auto previous_scratch=pending.scratch?pending.scratch->getDesc().size:0;
  const auto extra=sizes.accelerationStructureSize+native.size()+
      (scratch_size>previous_scratch?scratch_size-previous_scratch:0);
  check(pending.build_bytes<=config.maximum_build_bytes&&extra<=config.maximum_build_bytes-pending.build_bytes,"ray TLAS build budget exceeded");
  const auto used=live_bytes()+pending.build_bytes;
  check(used<=config.maximum_resident_bytes&&extra<=config.maximum_resident_bytes-used,"ray TLAS resident budget exceeded");
  pending.instances=buffer(device,native.size(),static_cast<std::uint32_t>(stride),rhi::BufferUsage::AccelerationStructureBuildInput);
  input.instances.instanceBuffer=pending.instances;
  pending.build_bytes+=extra;
  scene.tlas=acceleration(device,rhi::AccelerationStructureKind::TopLevel,sizes.accelerationStructureSize);scene.bytes+=sizes.accelerationStructureSize;
  if(scratch_size>previous_scratch)pending.scratch=buffer(device,scratch_size,4,rhi::BufferUsage::UnorderedAccess);
  checked(commands->uploadBufferData(pending.instances,0,native.size(),native.data()),"ray instances upload failed");commands->globalBarrier();
  commands->buildAccelerationStructure(desc,scene.tlas,nullptr,pending.scratch,0,nullptr);commands->globalBarrier();
}
bool RayGeometry::record(rhi::ICommandEncoder* commands,rhi::IBuffer* pool,std::span<const GpuPage> pages,const SelectionView& view) {
  return record_impl(commands,pool,pages,view,{});
}
bool RayGeometry::record(rhi::ICommandEncoder* commands,rhi::IBuffer* pool,std::span<const GpuPage> pages,
    std::span<const InstanceSelectionView> views) {
  if(!valid_instance_selection(views)) {state_->error="invalid ray instance view set";return false;}
  return record_impl(commands,pool,pages,{},views);
}
bool RayGeometry::record_impl(rhi::ICommandEncoder* commands,rhi::IBuffer* pool,std::span<const GpuPage> pages,
    const SelectionView& view,std::span<const InstanceSelectionView> views) {
  auto& s=*state_;try {
    check(s.device&&commands&&pool,"ray geometry is not initialized");check(!s.pending,"ray replacement already pending");
    check(pool->getDesc().size<=UINT32_MAX,"ray page pool exceeds 32-bit byte addressing");
    s.budget={};
    if(s.config.scheduler && !s.config.scheduler->acquire(&s)) {
      s.error=s.config.scheduler->error();s.budget.deferred=s.error.empty();return false;
    }
    SelectionResult selected;bool selected_ok{};
    if(views.empty()) {
      SelectionView ray_view=view;ray_view.frustum=false;ray_view.error_pixels=std::max(view.error_pixels,s.config.error_pixels);
      selected_ok=select_geometry(s.topology,pages,ray_view,s.config.maximum_clusters,s.config.feedback_capacity,selected,s.error);
    } else {
      std::vector<InstanceSelectionView> ray_views(views.begin(),views.end());
      for(auto& ray_view:ray_views) {ray_view.flags[0]=0;ray_view.error_scale[0]=std::max(ray_view.error_scale[0],s.config.error_pixels);}
      selected_ok=select_geometry_instances(s.topology,pages,ray_views,s.config.maximum_clusters,s.config.feedback_capacity,selected,s.error);
    }
    s.feedback=std::move(selected.requests);
    if(!selected_ok)throw std::runtime_error(s.error);check(!selected.clusters.empty(),"empty ray coverage cut");
    auto next=std::make_unique<State::Build>();next->scene=std::make_shared<RaySnapshot>();auto& scene=*next->scene;
    scene.source_hash=s.asset.source_hash;scene.clusters=spatial_order(s.asset,std::move(selected.clusters));
    scene.vertex_stride=std::all_of(scene.clusters.begin(),scene.clusters.end(),[&](auto id) {
      return (s.asset.clusters[id].flags&geometry_position_only)!=0;
    })?12:ray_vertex_bytes;
    std::vector<RayCopyCluster> copies;std::uint32_t vertex_count=0,triangle_count=0;std::set<std::uint32_t> source_pages;
    for(std::size_t i=0;i<scene.clusters.size();++i) {
      const auto id=scene.clusters[i];const auto& c=s.asset.clusters[id];check(c.page<pages.size(),"ray page table range");const auto& page=pages[c.page];
      check(page.resident&&page.generation&&page.slot!=invalid_page,"ray source page missing");
      check(page.reserved<page_bytes && (page.reserved&15u)==0 &&
          std::uint64_t(c.vertex_offset)+std::uint64_t(c.vertex_count)*((c.flags&geometry_position_only)?12:sizeof(MapVertex))<=page_bytes-page.reserved &&
          std::uint64_t(c.triangle_offset)+std::uint64_t(c.triangle_count)*4<=page_bytes-page.reserved,"ray packed page range");
      check((std::uint64_t(page.slot)+1)*page_bytes<=pool->getDesc().size,"ray source pool range");
      if(source_pages.insert(c.page).second)next->source_pages.push_back({page.slot,page.generation});
      if(i%s.config.clusters_per_blas==0)scene.batches.push_back({triangle_count,0,static_cast<std::uint32_t>(i),0});
      auto& batch=scene.batches.back();batch.triangle_count+=c.triangle_count;++batch.cluster_count;
      copies.push_back({id,c.page,page.slot,page.generation,c.vertex_offset,c.triangle_offset,c.vertex_count,c.triangle_count,vertex_count,triangle_count,c.material,c.flags});
      vertex_count+=c.vertex_count;triangle_count+=c.triangle_count;
    }
    const auto vertex_bytes=std::uint64_t(vertex_count)*scene.vertex_stride,index_bytes=std::uint64_t(triangle_count)*12,record_bytes=std::uint64_t(triangle_count)*sizeof(RayTriangle);
    scene.bytes=vertex_bytes+index_bytes+record_bytes+scene.batches.size()*sizeof(RayBatch);
    auto& budget=s.budget;budget.expansion_bytes=scene.bytes;
    budget.clusters=static_cast<std::uint32_t>(scene.clusters.size());budget.triangles=triangle_count;
    budget.build_bytes=scene.bytes+copies.size()*sizeof(RayCopyCluster)+pages.size()*sizeof(GpuPage)+4;
    std::vector<rhi::AccelerationStructureSizes> blas_sizes;
    for(const auto& batch:scene.batches) {
      rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Triangles;auto& triangles=input.triangles;
      // Size queries need an address but never read source data. Allocate only
      // after the complete BLAS/TLAS peak has passed admission.
      triangles.vertexBuffers[0]=pool;triangles.vertexBufferCount=1;triangles.vertexFormat=rhi::Format::RGB32Float;triangles.vertexCount=vertex_count;triangles.vertexStride=scene.vertex_stride;
      triangles.indexBuffer={pool,0};triangles.indexCount=batch.triangle_count*3;triangles.indexFormat=rhi::IndexFormat::Uint32;
      bool opaque=true;
      for(std::uint32_t c=0;c<batch.cluster_count;++c)
        opaque=opaque && (s.asset.clusters[scene.clusters[batch.first_cluster+c]].flags&3u)==0;
      triangles.flags=opaque?rhi::AccelerationStructureGeometryFlags::Opaque:rhi::AccelerationStructureGeometryFlags::None;
      const auto desc=build_desc(&input,1,true);rhi::AccelerationStructureSizes sizes{};checked(s.device->getAccelerationStructureSizes(desc,&sizes),"ray BLAS sizes failed");
      budget.blas_bytes+=sizes.accelerationStructureSize;
      budget.maximum_blas_bytes=std::max(budget.maximum_blas_bytes,sizes.accelerationStructureSize);
      budget.scratch_bytes=std::max(budget.scratch_bytes,std::max<std::uint64_t>(sizes.scratchSize,4));
      budget.separate_scratch_bytes+=std::max<std::uint64_t>(sizes.scratchSize,4);
      blas_sizes.push_back(sizes);next->blas_inputs.push_back(input);
    }
    const auto stride=s.config.build_local_tlas?
        rhi::getAccelerationStructureInstanceDescSize(rhi::getAccelerationStructureInstanceDescType(s.device)):0;
    check(!s.config.build_local_tlas || stride>0,"ray instance layout unavailable");
    rhi::AccelerationStructureBuildInput instances{};instances.type=rhi::AccelerationStructureBuildInputType::Instances;
    instances.instances.instanceBuffer=pool;instances.instances.instanceStride=static_cast<std::uint32_t>(stride);
    instances.instances.instanceCount=static_cast<std::uint32_t>(scene.batches.size());
    const auto tlas_desc=build_desc(&instances,1,false);rhi::AccelerationStructureSizes tlas_sizes{};
    if(s.config.build_local_tlas) {
      checked(s.device->getAccelerationStructureSizes(tlas_desc,&tlas_sizes),"ray TLAS preflight failed");
      budget.scratch_bytes=std::max(budget.scratch_bytes,std::max<std::uint64_t>(tlas_sizes.scratchSize,4));
    }
    budget.build_bytes+=budget.blas_bytes+budget.scratch_bytes+tlas_sizes.accelerationStructureSize+scene.batches.size()*stride;
    budget.uncompacted_build_bytes=budget.build_bytes;
    budget.resident_bytes=s.live_bytes()+budget.build_bytes;
    budget.resident_limited=budget.resident_bytes>s.config.maximum_resident_bytes;
    budget.limited=budget.build_bytes>s.config.maximum_build_bytes||budget.resident_limited;
    next->tlas_reserve=tlas_sizes.accelerationStructureSize+scene.batches.size()*stride;
    const auto fixed_bytes=budget.build_bytes-budget.blas_bytes-next->tlas_reserve;
    if(budget.limited && !s.config.scheduler) {
      const auto minimum=fixed_bytes+next->tlas_reserve+2*budget.maximum_blas_bytes;
      next->staged=minimum<=s.config.maximum_build_bytes && s.live_bytes()+minimum<=s.config.maximum_resident_bytes;
      if(next->staged)budget.limited=budget.resident_limited=false;
    }
    check(!budget.limited,"ray complete cut exceeds memory budget");
    if(s.config.scheduler) {
      scene.allocation=s.config.scheduler->reserve(&s,budget.build_bytes-budget.scratch_bytes,budget.scratch_bytes);
      if(!scene.allocation) {
        check(s.config.scheduler->error().empty(),s.config.scheduler->error().c_str());
        budget.limited=budget.resident_limited=true;
        throw std::runtime_error("complete scene ray candidate exceeds aggregate memory budget");
      }
      // The full uncompacted cut is reserved, but bounded stages can lower its
      // final resident footprint without assuming unknown compaction ratios.
      next->staged=true;
    }
    scene.vertices=buffer(s.device,vertex_bytes,0,geometry);scene.indices=buffer(s.device,index_bytes,4,geometry);
    scene.triangle_records=buffer(s.device,record_bytes,sizeof(RayTriangle),storage);scene.batch_records=buffer(s.device,scene.batches.size()*sizeof(RayBatch),sizeof(RayBatch),storage,scene.batches.data());
    next->copies=buffer(s.device,copies.size()*sizeof(RayCopyCluster),sizeof(RayCopyCluster),storage,copies.data());
    next->pages=buffer(s.device,pages.size()*sizeof(GpuPage),sizeof(GpuPage),storage,pages.data());const std::uint32_t zero=0;next->validation=buffer(s.device,4,4,storage,&zero);
    if(s.config.scheduler)next->scratch=s.config.scheduler->scratch();
    else next->scratch=buffer(s.device,budget.scratch_bytes,4,rhi::BufferUsage::UnorderedAccess);
    scene.blas.resize(scene.batches.size());
    for(std::size_t i=0;i<scene.batches.size();++i) {
      auto& triangles=next->blas_inputs[i].triangles;triangles.vertexBuffers[0]=scene.vertices;
      triangles.indexBuffer={scene.indices.get(),std::uint64_t(scene.batches[i].first_triangle)*12};
      if(!next->staged)scene.blas[i]=acceleration(s.device,rhi::AccelerationStructureKind::BottomLevel,blas_sizes[i].accelerationStructureSize);
    }
    if(!next->staged)scene.bytes+=budget.blas_bytes;
    rhi::QueryPoolDesc query{};query.type=rhi::QueryType::AccelerationStructureCompactedSize;query.count=static_cast<std::uint32_t>(scene.blas.size());
    checked(s.device->createQueryPool(query,scene.compact_sizes.writeRef()),"ray compaction query creation failed");
    next->build_bytes=next->staged?fixed_bytes:budget.build_bytes-next->tlas_reserve;
    next->blas_sizes=std::move(blas_sizes);
    s.pending=std::move(next);auto& pending=*s.pending;
    auto pass=commands->beginComputePass();auto root=pass->bindPipeline(s.expand);check(root,"ray expand binding failed");
    bind(root,"copies",pending.copies);bind(root,"pageTable",pending.pages);bind(root,"pagePool",pool);bind(root,"rayVertices",scene.vertices);bind(root,"rayIndices",scene.indices);bind(root,"triangleRecords",scene.triangle_records);bind(root,"validation",pending.validation);
    const auto count=static_cast<std::uint32_t>(copies.size());checked(rhi::ShaderCursor(root)["copyCount"].setData(count),"ray copy count binding");
    checked(rhi::ShaderCursor(root)["vertexStride"].setData(scene.vertex_stride),"ray vertex stride binding");
    pass->dispatchCompute(count,1,1);pass->end();commands->globalBarrier();
    commands->setBufferState(scene.vertices,rhi::ResourceState::AccelerationStructureBuildInput);commands->setBufferState(scene.indices,rhi::ResourceState::AccelerationStructureBuildInput);
    if(pending.staged)s.stage_build(pending,commands);
    else for(std::uint32_t i=0;i<scene.blas.size();++i) {
      const auto desc=build_desc(&pending.blas_inputs[i],1,true);rhi::AccelerationStructureQueryDesc result{rhi::QueryType::AccelerationStructureCompactedSize,scene.compact_sizes.get(),i};
      commands->buildAccelerationStructure(desc,scene.blas[i],nullptr,pending.scratch,1,&result);
      // Every build reuses the same scratch range; serialize its UAV writes.
      commands->globalBarrier();
    }
    if(!pending.staged) {
      commands->globalBarrier();s.tlas(pending,commands);
      commands->setBufferState(scene.vertices,rhi::ResourceState::ShaderResource);commands->setBufferState(scene.indices,rhi::ResourceState::ShaderResource);
    }
    budget.build_bytes=pending.build_bytes;budget.resident_bytes=s.live_bytes()+pending.build_bytes;
    pending.recorded=true;s.error.clear();return true;
  } catch(const std::exception& e) {s.error=e.what();return false;}
}
}
