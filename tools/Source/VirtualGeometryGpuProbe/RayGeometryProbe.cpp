#include "RayGeometry.h"
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
GeometryAsset fixture(std::vector<std::uint8_t>& pool,float z) {
  GeometryAsset asset;asset.source_hash=std::string(64,'a');asset.source_triangles=17;asset.material_count=1;
  asset.pages.resize(1);asset.pages[0].encoded_size=page_bytes;asset.pages[0].checksum.fill('a');
  asset.roots={0};asset.group_pages={0};asset.groups.push_back({0,17,0,0,1,{{32,0,z},35,FLT_MAX}});pool.resize(page_bytes);
  for(unsigned i=0;i<17;++i) {
    GeometryCluster cluster;cluster.page=0;cluster.vertex_offset=i*256;cluster.vertex_count=3;
    cluster.triangle_offset=cluster.vertex_offset+240;cluster.triangle_count=1;cluster.flags=256;cluster.bounds={{float(i*4),0,z},1,0};
    asset.clusters.push_back(cluster);MapVertex vertices[3]{};const float xy[3][2]={{-.7f,-.7f},{0,.7f},{.7f,-.7f}};
    for(unsigned j=0;j<3;++j) {vertices[j].position[0]=float(i*4)+xy[j][0];vertices[j].position[1]=xy[j][1];vertices[j].position[2]=z;vertices[j].normal[2]=-1;}
    std::memcpy(pool.data()+cluster.vertex_offset,vertices,sizeof(vertices));const unsigned triangle=0x020100;
    std::memcpy(pool.data()+cluster.triangle_offset,&triangle,4);
  }
  return asset;
}
void finish(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder,RayGeometry* geometry=nullptr) {
  auto commands=encoder->finish();check(bool(commands),"ray probe commands");auto fence=device->createFence({});check(bool(fence),"ray probe fence");
  auto* command=commands.get();auto* signal=fence.get();std::uint64_t value=1;rhi::SubmitDesc submit{};
  submit.commandBuffers=&command;submit.commandBufferCount=1;submit.signalFences=&signal;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  checked(queue->submit(submit),"ray probe submit");if(geometry)owner(geometry->submitted(fence,value),*geometry);
  checked(device->waitForFences(1,&signal,&value,true,30'000'000'000ull),"ray probe timeout");
}
void trace(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::IComputePipeline* pipeline,const RaySnapshot& scene,float depth) {
  auto results=buffer(device,nullptr,18*16,16,true);check(bool(results),"ray results allocation");auto encoder=queue->createCommandEncoder();
  auto pass=encoder->beginComputePass();auto root=pass->bindPipeline(pipeline);check(root,"ray query pipeline binding");
  rhi::ShaderCursor cursor(root);checked(cursor["scene"].setBinding(rhi::Binding(scene.tlas)),"ray scene binding");
  checked(cursor["batches"].setBinding(rhi::Binding(scene.batch_records)),"ray batch binding");
  checked(cursor["triangles"].setBinding(rhi::Binding(scene.triangle_records)),"ray triangle binding");
  checked(cursor["results"].setBinding(rhi::Binding(results)),"ray results binding");pass->dispatchCompute(1,1,1);pass->end();finish(device,queue,encoder);
  std::array<std::array<unsigned,4>,18> data{};checked(device->readBuffer(results,0,sizeof(data),data.data()),"ray query readback");
  for(unsigned i=0;i<17;++i) {
    float distance{};std::memcpy(&distance,&data[i][3],4);
    // Hit identity is exact; ray T carries normal GPU intersection rounding.
    if(!(data[i][0]==i&&data[i][1]==0&&data[i][2]==0&&std::abs(distance-depth)<=1e-6f*depth)) {
      std::fprintf(stderr,"ray_debug lane=%u got=%u,%u,%u distance=%.9g want=%.9g\n",i,data[i][0],data[i][1],data[i][2],distance,depth);
      check(false,"ray hit geometry mapping/depth mismatch");
    }
  }
  check(data[17][0]==UINT32_MAX,"ray miss produced a hit");
}
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
    ComPtr<rhi::IComputePipeline> query;check(create_rhi_compute_pipeline(device,queries,"probeRays",query),"ray query shader compilation");trace(device,queue,query,*first,2);
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
    checked(retained->setCurrentValue(1),"ray consumer completion");geometry.poll();check(weak.expired(),"completed ray snapshot was not retired");
    auto stable=geometry.snapshot();const GpuPage missing{0,1,0,0};encoder=queue->createCommandEncoder();
    check(!geometry.record(encoder,replacement,std::span(&missing,1),view)&&geometry.snapshot()==stable&&!geometry.requests().empty(),"missing roots replaced complete ray coverage");
    RayGeometry bounded;auto small=config;small.maximum_build_bytes=4;owner(bounded.initialize(device,asset,expansion,small),bounded);
    encoder=queue->createCommandEncoder();check(!bounded.record(encoder,pool,std::span(&page,1),view)&&!bounded.snapshot(),"ray build budget not enforced");
    std::printf("ray_geometry=passed triangles=17 spatial_blas=2 offscreen=covered replacement=atomic retained_fence=passed compacted=%u\n",unsigned(compact));return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"ray_geometry=failed reason=%s\n",error.what());return false;}
}
