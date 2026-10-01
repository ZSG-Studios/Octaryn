#include "RayGeometry.h"
#include "InstanceSelection.h"
#include "SceneRayScheduler.h"
#include "RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
bool probe_map_ray_materials(rhi::IDevice*,rhi::ICommandQueue*,const RaySnapshot&,const char*);
namespace {
void check(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
void owner(bool value,const RayGeometry& geometry) {if(!value)throw std::runtime_error(geometry.error());}
void checked(SlangResult result,const char* error) {check(SLANG_SUCCEEDED(result),error);}
ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,const void* data,std::uint64_t size,unsigned stride,bool write=false) {
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  if(write)desc.usage|=rhi::BufferUsage::UnorderedAccess;
  return device->createBuffer(desc,data);
}
GeometryAsset fixture(std::vector<std::uint8_t>& pool,float z,unsigned count=17,bool positions_only=false) {
  GeometryAsset asset;asset.source_hash=std::string(64,'a');asset.source_triangles=count;asset.material_count=3;
  asset.pages.resize(1);asset.pages[0].encoded_size=page_bytes;asset.pages[0].checksum.fill('a');
  pool.resize(page_bytes);
  for(unsigned i=0;i<count;++i) {
    asset.roots.push_back(i);asset.group_pages.push_back(0);
    asset.groups.push_back({i,1,0,i,1,{{float(i*4),0,z},1,FLT_MAX}});
    GeometryCluster cluster;cluster.group=i;cluster.page=0;cluster.vertex_offset=i*256;cluster.vertex_count=3;
    cluster.material=i%3;
    cluster.triangle_offset=cluster.vertex_offset+240;cluster.triangle_count=1;
    const bool compact=positions_only || i==16;
    if(compact)cluster.triangle_offset=cluster.vertex_offset+36;
    cluster.flags=256|(cluster.material==2?1u:0u)|(compact?512u:0u);cluster.bounds={{float(i*4),0,z},1,0};
    asset.clusters.push_back(cluster);MapVertex vertices[3]{};const float xy[3][2]={{-.7f,-.7f},{0,.7f},{.7f,-.7f}};
    for(unsigned j=0;j<3;++j) {
      auto& v=vertices[j];v.position[0]=float(i*4)+xy[j][0];v.position[1]=xy[j][1];v.position[2]=z;v.normal[2]=-1;
      v.uv[0]=.125f;v.uv[1]=.25f;v.uv1[0]=.5f;v.uv1[1]=.75f;
      v.tangent[0]=1;v.tangent[3]=-1;v.color[0]=.25f;v.color[1]=.5f;v.color[2]=.75f;v.color[3]=1;
      v.padding[0]=99;v.padding[1]=-99;
    }
    if(compact)for(unsigned j=0;j<3;++j)std::memcpy(pool.data()+cluster.vertex_offset+j*12,vertices[j].position,12);
    else std::memcpy(pool.data()+cluster.vertex_offset,vertices,sizeof(vertices));const unsigned triangle=0x020100;
    std::memcpy(pool.data()+cluster.triangle_offset,&triangle,4);
  }
  return asset;
}
void attributes(rhi::IDevice* device,const RaySnapshot& scene,const GeometryAsset& asset,const std::vector<std::uint8_t>& source) {
  std::vector<std::uint8_t> actual(scene.vertices->getDesc().size);
  checked(device->readBuffer(scene.vertices,0,actual.size(),actual.data()),"ray authored attribute readback");
  std::size_t offset{};
  for(const auto id:scene.clusters) {
    const auto& cluster=asset.clusters[id];
    for(unsigned vertex=0;vertex<cluster.vertex_count;++vertex) {
      MapVertex expected{};
      if(cluster.flags&geometry_position_only)std::memcpy(expected.position,source.data()+cluster.vertex_offset+vertex*12,12);
      else std::memcpy(&expected,source.data()+cluster.vertex_offset+vertex*sizeof(MapVertex),sizeof(MapVertex));
      if(scene.vertex_stride==12)check(std::memcmp(actual.data()+offset,expected.position,12)==0,"ray storage changed compact positions");
      else check(std::memcmp(actual.data()+offset,&expected,40)==0 &&
          std::memcmp(actual.data()+offset+40,expected.tangent,32)==0,"ray storage changed authored attribute bits");
      offset+=scene.vertex_stride;
    }
  }
  check(offset==actual.size(),"ray attribute storage stride mismatch");
}
void finish(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder,RayGeometry* geometry=nullptr) {
  auto commands=encoder->finish();check(bool(commands),"ray probe commands");auto fence=device->createFence({});check(bool(fence),"ray probe fence");
  auto* command=commands.get();auto* signal=fence.get();std::uint64_t value=1;rhi::SubmitDesc submit{};
  submit.commandBuffers=&command;submit.commandBufferCount=1;submit.signalFences=&signal;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  checked(queue->submit(submit),"ray probe submit");if(geometry)owner(geometry->submitted(fence,value),*geometry);
  checked(device->waitForFences(1,&signal,&value,true,30'000'000'000ull),"ray probe timeout");
}
void trace(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::IComputePipeline* pipeline,const RaySnapshot& scene,float depth) {
  const auto count=static_cast<unsigned>(scene.clusters.size());
  auto results=buffer(device,nullptr,(count+1)*16,16,true);check(bool(results),"ray results allocation");auto encoder=queue->createCommandEncoder();
  auto pass=encoder->beginComputePass();auto root=pass->bindPipeline(pipeline);check(root,"ray query pipeline binding");
  rhi::ShaderCursor cursor(root);checked(cursor["scene"].setBinding(rhi::Binding(scene.tlas)),"ray scene binding");
  checked(cursor["batches"].setBinding(rhi::Binding(scene.batch_records)),"ray batch binding");
  checked(cursor["triangles"].setBinding(rhi::Binding(scene.triangle_records)),"ray triangle binding");
  checked(cursor["results"].setBinding(rhi::Binding(results)),"ray results binding");
  checked(cursor["probeCount"].setData(count),"ray result count binding");pass->dispatchCompute((count+32)/32,1,1);pass->end();finish(device,queue,encoder);
  std::vector<std::array<unsigned,4>> data(count+1);checked(device->readBuffer(results,0,data.size()*16,data.data()),"ray query readback");
  for(unsigned i=0;i<count;++i) {
    float distance{};std::memcpy(&distance,&data[i][3],4);
    // Hit identity is exact; ray T carries normal GPU intersection rounding.
    const auto& batch=scene.batches[i/16];unsigned candidate=0;
    for(unsigned cluster=0;cluster<batch.cluster_count;++cluster)
      if(scene.clusters[batch.first_cluster+cluster]%3==2)candidate=0x80000000u;
    if(!(data[i][0]==i&&data[i][1]==candidate&&data[i][2]==i%3&&std::abs(distance-depth)<=1e-6f*depth)) {
      std::fprintf(stderr,"ray_debug lane=%u got=%u,%u,%u distance=%.9g want=%.9g\n",i,data[i][0],data[i][1],data[i][2],distance,depth);
      check(false,"ray hit geometry mapping/depth mismatch");
    }
  }
  check(data[count][0]==UINT32_MAX,"ray miss produced a hit");
}
void staged(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* expansion,rhi::IComputePipeline* query) {
  std::vector<std::uint8_t> bytes;auto asset=fixture(bytes,2,65);
  auto pool=buffer(device,bytes.data(),bytes.size(),0);const GpuPage page{0,1,1,0};SelectionView view{{0,0,-10},100,1};
  RayGeometry measure;RayGeometryConfig config;config.clusters_per_blas=16;
  owner(measure.initialize(device,asset,expansion,config),measure);auto encoder=queue->createCommandEncoder();
  owner(measure.record(encoder,pool,std::span(&page,1),view),measure);
  const auto full=measure.budget();encoder.setNull();measure.cancel_unsubmitted();
  config.maximum_build_bytes=full.uncompacted_build_bytes-full.maximum_blas_bytes;
  config.maximum_resident_bytes=2*config.maximum_build_bytes;
  RayGeometry geometry;owner(geometry.initialize(device,asset,expansion,config),geometry);
  encoder=queue->createCommandEncoder();owner(geometry.record(encoder,pool,std::span(&page,1),view),geometry);
  unsigned submissions=0;std::uint64_t peak{};
  for(;;) {
    check(!geometry.snapshot(),"partial staged ray cut was published");
    peak=std::max(peak,geometry.gpu_bytes());check(peak<=config.maximum_build_bytes,"staged ray build exceeded peak budget");
    finish(device,queue,encoder,&geometry);++submissions;
    if(geometry.poll())break;
    check(geometry.error().empty() && geometry.continuation_ready() && submissions<32,"staged ray build stalled");
    encoder=queue->createCommandEncoder();owner(geometry.record_continue(encoder),geometry);
  }
  check(submissions>2 && geometry.snapshot()->clusters.size()==65,"staged ray cut coverage incomplete");
  trace(device,queue,query,*geometry.snapshot(),2);
  const auto previous=geometry.snapshot();fixture(bytes,3,65);
  auto replacement=buffer(device,bytes.data(),bytes.size(),0);
  encoder=queue->createCommandEncoder();owner(geometry.record(encoder,replacement,std::span(&page,1),view),geometry);
  unsigned replacement_submissions=0;
  for(;;) {
    check(geometry.snapshot()==previous,"staged replacement changed the published cut early");
    check(geometry.budget().build_bytes<=config.maximum_build_bytes &&
        geometry.gpu_bytes()<=config.maximum_resident_bytes,"staged replacement exceeded resident budget");
    finish(device,queue,encoder,&geometry);++replacement_submissions;
    if(geometry.poll())break;
    check(geometry.error().empty() && geometry.continuation_ready() && replacement_submissions<32,
        "staged ray replacement stalled");
    if(replacement_submissions==1)trace(device,queue,query,*previous,2);
    encoder=queue->createCommandEncoder();owner(geometry.record_continue(encoder),geometry);
  }
  trace(device,queue,query,*previous,2);trace(device,queue,query,*geometry.snapshot(),3);
  std::printf("ray_geometry_staged=passed triangles=65 complete=1 replacement=atomic submissions=%u peak=%llu budget=%llu uncompacted=%llu\n",submissions,
      static_cast<unsigned long long>(peak),static_cast<unsigned long long>(config.maximum_build_bytes),
      static_cast<unsigned long long>(full.uncompacted_build_bytes));
}
void scene_scheduler(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* expansion) {
  std::vector<std::uint8_t> bytes;auto asset=fixture(bytes,2,17,true);
  std::vector<std::uint8_t> packed(page_bytes,0xff);
  const auto& last=asset.clusters.back();const auto used=last.triangle_offset+last.triangle_count*4;
  std::copy_n(bytes.begin(),used,packed.begin()+8192);
  std::copy_n(bytes.begin(),used,packed.begin()+16384);
  auto pool=buffer(device,packed.data(),packed.size(),0);
  const GpuPage page{0,1,1,8192},second_page{0,2,1,16384};SelectionView view{{0,0,-10},100,1};
  auto ledger=std::make_shared<SceneMemoryLedger>(16ull<<20);
  auto scheduler=std::make_shared<SceneRayScheduler>(ledger);
  RayGeometryConfig config;config.clusters_per_blas=16;config.build_local_tlas=false;config.scheduler=scheduler;
  bool publish=true;config.admit_publication=[&](auto,std::string&){return publish;};
  RayGeometry first,second;owner(first.initialize(device,asset,expansion,config),first);
  asset.source_hash=std::string(64,'b');owner(second.initialize(device,asset,expansion,config),second);
  auto encoder=queue->createCommandEncoder();owner(first.record(encoder,pool,std::span(&page,1),view),first);
  const auto reserved=ledger->stats().used;auto* scratch=scheduler->scratch();
  auto waiting=queue->createCommandEncoder();
  check(!second.record(waiting,pool,std::span(&second_page,1),view) && second.budget().deferred &&
      ledger->stats().used==reserved,"concurrent scene build bypassed exclusive scratch admission");
  waiting.setNull();second.cancel_unsubmitted();
  const auto complete=[&](RayGeometry& geometry,ComPtr<rhi::ICommandEncoder> commands) {
    for(unsigned stages=0;;++stages) {
      check(stages<32,"scene ray staged build did not finish");
      finish(device,queue,commands,&geometry);
      if(geometry.poll())break;
      if(geometry.budget().deferred)return false;
      check(geometry.error().empty() && geometry.continuation_ready(),"scene ray continuation unavailable");
      commands=queue->createCommandEncoder();owner(geometry.record_continue(commands),geometry);
    }
    check(geometry.snapshot() && !geometry.snapshot()->tlas,"scene owner allocated redundant local TLAS");
    return true;
  };
  check(complete(first,std::move(encoder)),"first scene cut deferred unexpectedly");
  encoder=queue->createCommandEncoder();owner(second.record(encoder,pool,std::span(&second_page,1),view),second);
  check(complete(second,std::move(encoder)),"second scene cut deferred unexpectedly");
  check(scheduler->scratch()==scratch,"equal-size scene builds did not share scratch");
  check(ledger->stats().used==first.snapshot()->bytes+second.snapshot()->bytes+scratch->getDesc().size,
      "scene allocation totals duplicate scratch or omit published BLAS");
  check(probe_map_ray_materials(device,queue,*first.snapshot(),OCTARYN_MAP_RAY_PROBE_SHADER),
      "shared object BLAS failed world TLAS material traversal");
  attributes(device,*first.snapshot(),asset,bytes);attributes(device,*second.snapshot(),asset,bytes);
  auto previous=first.snapshot();const auto before=ledger->stats().used;
  auto pressure=ledger->reserve(ledger->stats().limit-before,SceneMemoryDomain::Pages);
  encoder=queue->createCommandEncoder();
  check(!first.record(encoder,pool,std::span(&page,1),view) && first.budget().limited &&
      first.snapshot()==previous && ledger->stats().used==ledger->stats().limit,
      "aggregate pressure published a hole or allocated an unadmitted candidate");
  encoder.setNull();first.cancel_unsubmitted();pressure.reset();
  publish=false;encoder=queue->createCommandEncoder();owner(first.record(encoder,pool,std::span(&page,1),view),first);
  check(!complete(first,std::move(encoder)) && first.snapshot()==previous && ledger->stats().used==before,
      "world TLAS admission denial replaced the complete ray cut or leaked its candidate");
  publish=true;
  auto consumer=device->createFence({});owner(first.reference(previous,consumer,1),first);
  encoder=queue->createCommandEncoder();owner(first.record(encoder,pool,std::span(&page,1),view),first);
  check(complete(first,std::move(encoder)),"scene replacement deferred unexpectedly");
  check(first.snapshot()!=previous && ledger->stats().phase_bytes[unsigned(SceneMemoryPhase::Retired)]==previous->bytes,
      "old world TLAS consumer lost its retired allocation charge");
  const auto with_retired=ledger->stats().used;const auto old_bytes=previous->bytes;previous.reset();first.poll();
  check(ledger->stats().used==with_retired,"unsignaled consumer released retired scene ray allocation");
  checked(consumer->setCurrentValue(1),"scene ray consumer fence");first.poll();
  check(ledger->stats().used==with_retired-old_bytes,"completed world TLAS consumer did not release retired charge");
  const auto stable=ledger->stats().used;const GpuPage bad{0,1,1,65520};
  encoder=queue->createCommandEncoder();
  check(!first.record(encoder,pool,std::span(&bad,1),view) && ledger->stats().used==stable,
      "out-of-range packed root allocated GPU memory");
  encoder.setNull();first.cancel_unsubmitted();
  check(ledger->stats().peak<=ledger->stats().limit,"scene ray aggregate budget exceeded");
  std::printf("scene_ray_scheduler=passed assets=2 scratch=shared local_tlas=0 packed_roots=2 world_materials=passed complete_candidate=1 tlas_admission=retained retired=charged peak=%llu budget=%llu\n",
      static_cast<unsigned long long>(ledger->stats().peak),static_cast<unsigned long long>(ledger->stats().limit));
}
}
bool probe_ray_size_query(rhi::IDevice* device) {
  try {
    rhi::BufferDesc address_desc{};address_desc.size=64;address_desc.usage=rhi::BufferUsage::AccelerationStructureBuildInput;
    address_desc.defaultState=rhi::ResourceState::AccelerationStructureBuildInput;
    auto address=device->createBuffer(address_desc);check(bool(address),"ray size-query address");
    for(bool opaque:{false,true})for(unsigned vertices:{3'220'061u,49'152u,16'384u}) {
      rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Triangles;
      auto& triangles=input.triangles;triangles.vertexBuffers[0]=address;triangles.vertexBufferCount=1;
      triangles.vertexCount=vertices;triangles.vertexStride=sizeof(MapVertex);triangles.vertexFormat=rhi::Format::RGB32Float;
      triangles.indexBuffer=address;triangles.indexFormat=rhi::IndexFormat::Uint32;triangles.indexCount=16'384*3;
      triangles.flags=opaque?rhi::AccelerationStructureGeometryFlags::Opaque:rhi::AccelerationStructureGeometryFlags::None;
      rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
      build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowCompaction;
      rhi::AccelerationStructureSizes sizes{};checked(device->getAccelerationStructureSizes(build,&sizes),"ray diagnostic sizes");
      std::printf("ray_size_query vertices=%u triangles=16384 opaque=%u blas=%llu scratch=%llu\n",vertices,unsigned(opaque),
          static_cast<unsigned long long>(sizes.accelerationStructureSize),static_cast<unsigned long long>(sizes.scratchSize));
    }
    return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"ray_size_query=failed reason=%s\n",error.what());return false;}
}
bool probe_ray_geometry(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* expansion,const char* queries) {
  try {
    check(device->hasFeature(rhi::Feature::RayQuery),"ray query unavailable");std::vector<std::uint8_t> bytes;auto asset=fixture(bytes,2);
    RayGeometry geometry;RayGeometryConfig config;config.clusters_per_blas=16;owner(geometry.initialize(device,asset,expansion,config),geometry);
    auto pool=buffer(device,bytes.data(),bytes.size(),0);check(bool(pool),"ray page pool allocation");const GpuPage page{0,1,1,0};
    // A raster frustum rejecting the whole fixture must not remove offscreen ray coverage.
    SelectionView view{{0,0,-10},100,1};view.frustum=true;view.planes[0][0]=1;view.planes[0][3]=-1000;
    auto encoder=queue->createCommandEncoder();owner(geometry.record(encoder,pool,std::span(&page,1),view),geometry);
    check(!geometry.snapshot()&&geometry.pending_pages().size()==1,"ray build published before submission");finish(device,queue,encoder,&geometry);owner(geometry.poll(),geometry);
    auto first=geometry.snapshot();check(first&&first->clusters.size()==17&&first->batches.size()==2,"ray spatial batch coverage");
    attributes(device,*first,asset,bytes);
    ComPtr<rhi::IComputePipeline> query;check(create_rhi_compute_pipeline(device,queries,"probeRays",query),"ray query shader compilation");trace(device,queue,query,*first,2);
    staged(device,queue,expansion,query);
    scene_scheduler(device,queue,expansion);
    check(probe_map_ray_materials(device,queue,*first,OCTARYN_MAP_RAY_PROBE_SHADER),"production cluster material traversal");
    {
      std::vector<std::uint8_t> compact_bytes;auto compact_asset=fixture(compact_bytes,2,17,true);
      auto compact_pool=buffer(device,compact_bytes.data(),compact_bytes.size(),0);RayGeometry compact_geometry;
      owner(compact_geometry.initialize(device,compact_asset,expansion,config),compact_geometry);
      GeometryTransform far,near;far.world[11]=10000;
      near.world={-2,.5f,0,0,0,3,0,0,0,0,.25f,0};
      const std::array views{instance_selection_view(view,far),instance_selection_view(view,near)};
      encoder=queue->createCommandEncoder();
      owner(compact_geometry.record(encoder,compact_pool,std::span(&page,1),std::span(views)),compact_geometry);
      finish(device,queue,encoder,&compact_geometry);owner(compact_geometry.poll(),compact_geometry);
      auto compact_scene=compact_geometry.snapshot();check(compact_scene->vertex_stride==12,"compact ray positions were expanded");
      check(compact_scene->clusters.size()==17 && views[0].flags[0] && views[1].flags[0],
          "instance ray selection lost offscreen roots or modified caller raster views");
      attributes(device,*compact_scene,compact_asset,compact_bytes);trace(device,queue,query,*compact_scene,2);
      check(probe_map_ray_materials(device,queue,*compact_scene,OCTARYN_MAP_RAY_PROBE_SHADER),"production compact ray material traversal");
      std::printf("ray_geometry_positions=passed stride=12 generated_normal=1 material_transform=passed instance_union=2 offscreen=complete\n");
    }
    // Admit exactly one measured build. A replacement must leave the complete
    // published scene traceable when retained resources consume that budget.
    RayGeometry pressure;auto capped=config;capped.maximum_build_bytes=geometry.budget().build_bytes;
    capped.maximum_resident_bytes=capped.maximum_build_bytes;
    owner(pressure.initialize(device,asset,expansion,capped),pressure);
    encoder=queue->createCommandEncoder();owner(pressure.record(encoder,pool,std::span(&page,1),view),pressure);
    finish(device,queue,encoder,&pressure);owner(pressure.poll(),pressure);auto complete=pressure.snapshot();
    const auto retained_bytes=pressure.gpu_bytes();encoder=queue->createCommandEncoder();
    check(!pressure.record(encoder,pool,std::span(&page,1),view)&&pressure.budget().resident_limited&&
        pressure.snapshot()==complete&&pressure.gpu_bytes()==retained_bytes&&pressure.pending_pages().empty(),"ray pressure changed complete coverage or allocated a rejected build");
    encoder.setNull();pressure.cancel_unsubmitted();trace(device,queue,query,*complete,2);
    auto retained=device->createFence({});owner(geometry.reference(first,retained,1),geometry);
    encoder=queue->createCommandEncoder();const bool compact=geometry.record_compaction(encoder);
    if(compact) {check(geometry.snapshot()==first,"ray compaction published early");finish(device,queue,encoder,&geometry);owner(geometry.poll(),geometry);trace(device,queue,query,*geometry.snapshot(),2);}
    else check(geometry.error().empty(),geometry.error().c_str());
    // Mutating streamed source pages after extraction must not alter an existing snapshot.
    fixture(bytes,3);auto replacement=buffer(device,bytes.data(),bytes.size(),0);auto previous=geometry.snapshot();
    encoder=queue->createCommandEncoder();owner(geometry.record(encoder,replacement,std::span(&page,1),view),geometry);
    check(geometry.snapshot()==previous,"ray replacement published early");finish(device,queue,encoder,&geometry);owner(geometry.poll(),geometry);
    trace(device,queue,query,*first,2);trace(device,queue,query,*geometry.snapshot(),3);
    std::weak_ptr<const RaySnapshot> weak=first;first.reset();previous.reset();check(!weak.expired(),"ray consumer fence failed to retain snapshot");
    check(geometry.gpu_bytes()>geometry.snapshot()->bytes,"retained ray frame omitted from replacement admission bytes");
    checked(retained->setCurrentValue(1),"ray consumer completion");geometry.poll();check(weak.expired(),"completed ray snapshot was not retired");
    check(geometry.gpu_bytes()==geometry.snapshot()->bytes,"completed ray frame still blocks replacement admission");
    auto stable=geometry.snapshot();const GpuPage missing{0,1,0,0};encoder=queue->createCommandEncoder();
    check(!geometry.record(encoder,replacement,std::span(&missing,1),view)&&geometry.snapshot()==stable&&!geometry.requests().empty(),"missing roots replaced complete ray coverage");
    RayGeometry bounded;auto small=config;small.maximum_build_bytes=4;owner(bounded.initialize(device,asset,expansion,small),bounded);
    encoder=queue->createCommandEncoder();check(!bounded.record(encoder,pool,std::span(&page,1),view)&&!bounded.snapshot(),"ray build budget not enforced");
    std::printf("ray_geometry=passed triangles=17 spatial_blas=2 opaque_batch=1 masked_batch=1 offscreen=covered replacement=atomic pressure=retained_complete retained_fence=passed compacted=%u\n",unsigned(compact));return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"ray_geometry=failed reason=%s\n",error.what());return false;}
}
