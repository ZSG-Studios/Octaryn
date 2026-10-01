#include "SceneRasterTables.h"
#include "SceneMemoryLedger.h"
#include "HybridRenderer.h"
#include "MapRendererInternal.h"
#include "WorldHdr.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,const void* data,std::size_t bytes,unsigned stride,bool indirect=false) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  if(indirect)desc.usage|=rhi::BufferUsage::IndirectArgument;
  auto result=device->createBuffer(desc,data);check(bool(result),"scene raster fixture buffer");return result;
}
struct Target {
  std::array<ComPtr<rhi::ITexture>,6> colors;
  std::array<ComPtr<rhi::ITextureView>,6> views;
  ComPtr<rhi::ITexture> depth;ComPtr<rhi::ITextureView> depth_view;
  explicit Target(rhi::IDevice* device) {
    rhi::TextureDesc desc{};desc.size={128,128,1};desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
    desc.defaultState=rhi::ResourceState::RenderTarget;
    for(unsigned i=0;i<6;++i) {desc.format=world_gbuffer_formats[i];colors[i]=device->createTexture(desc);check(bool(colors[i]),"scene raster target");views[i]=colors[i]->getDefaultView();}
    desc.format=rhi::Format::D32Float;desc.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::CopySource;
    desc.defaultState=rhi::ResourceState::DepthWrite;depth=device->createTexture(desc);check(bool(depth),"scene raster depth");depth_view=depth->getDefaultView();
  }
  void resolve(rhi::ICommandEncoder* encoder,HybridRenderer& renderer,const HybridInputs& input,bool clear) {
    std::array<rhi::RenderPassColorAttachment,6> attachments;
    for(unsigned i=0;i<6;++i) {attachments[i].view=views[i];attachments[i].loadOp=clear?rhi::LoadOp::Clear:rhi::LoadOp::Load;}
    rhi::RenderPassDepthStencilAttachment depth{};depth.view=depth_view;depth.depthClearValue=1;
    depth.depthLoadOp=clear?rhi::LoadOp::Clear:rhi::LoadOp::Load;
    rhi::RenderPassDesc desc{};desc.colorAttachments=attachments.data();desc.colorAttachmentCount=6;desc.depthStencilAttachment=&depth;
    auto* pass=encoder->beginRenderPass(desc);check(pass && renderer.resolve(pass,input),"scene raster material resolve");pass->end();
  }
};
}
bool probe_scene_raster(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* directory) {
  try {
    const SceneRasterCapacity capacity{2,2,3,3};const auto bytes=SceneRasterTables::required_bytes(capacity);
    auto ledger=std::make_shared<SceneMemoryLedger>(bytes);SceneRasterTables tables;
    check(tables.initialize(device,ledger,directory),tables.error().c_str());check(tables.reserve(capacity),tables.error().c_str());
    check(ledger->stats().used==bytes && !tables.reserve({3,3,4,4}) && ledger->stats().used==bytes,"scene raster growth ignored retained-bank peak");
    HybridRenderer baseline,batch;
    check(baseline.initialize(device,directory,world_gbuffer_formats,rhi::Format::D32Float) &&
        batch.initialize(device,directory,world_gbuffer_formats,rhi::Format::D32Float,true),"scene raster programs");
    check(baseline.resize(device,128,128) && batch.resize(device,128,128),"scene raster visibility targets");
    Target reference(device),observed(device);
    GeometryCluster cluster;cluster.vertex_count=3;cluster.triangle_count=1;cluster.triangle_offset=240;cluster.bounds={{0,0,1},.3f,0};
    auto clusters=buffer(device,&cluster,sizeof(cluster),sizeof(cluster));
    std::vector<std::uint8_t> payload(page_bytes);
    MapVertex vertices[3]{};const float xy[]{-.2f,-.2f,0,.2f,.2f,-.2f};
    for(unsigned i=0;i<3;++i) {vertices[i].position[0]=xy[i*2];vertices[i].position[1]=xy[i*2+1];vertices[i].position[2]=1;
      vertices[i].normal[2]=-1;vertices[i].tangent[0]=vertices[i].tangent[3]=1;}
    const std::uint32_t triangle=0x020100;
    for(unsigned offset:{256u,512u}) {std::memcpy(payload.data()+offset,vertices,sizeof(vertices));std::memcpy(payload.data()+offset+240,&triangle,4);}
    auto pool=buffer(device,payload.data(),payload.size(),0);
    const std::array<GpuPage,2> pages{{{0,11,1,256},{0,22,1,512}}};
    auto page0=buffer(device,&pages[0],16,16),page1=buffer(device,&pages[1],16,16);
    const std::uint32_t selected0[]{0,0,0,11},selected1[]{0,0,0,22},counter[]{1,0,0,0,0,0},dispatch[]{1,1,1,1,1,1};
    auto selection0=buffer(device,selected0,16,16),selection1=buffer(device,selected1,16,16);
    auto counters=buffer(device,counter,sizeof(counter),4),arguments=buffer(device,dispatch,sizeof(dispatch),4,true);
    std::array<MapRayMaterial,17> materials;materials[0].base_color[0]=1;materials[0].base_color[1]=.05f;
    materials[16].base_color[0]=.05f;materials[16].base_color[1]=1;
    auto material_buffer=buffer(device,materials.data(),sizeof(materials),sizeof(MapRayMaterial));
    std::array<GeometryTransform,2> first;GeometryTransform mirrored;std::string error;
    auto matrix=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,-.45f,.35f,0,1};
    check(geometry_transform(matrix,first[0],error),error.c_str());matrix[12]=.45f;
    check(geometry_transform(matrix,first[1],error),error.c_str());matrix[0]=-1.1f;matrix[4]=.15f;matrix[5]=.8f;matrix[12]=0;matrix[13]=-.4f;
    check(geometry_transform(matrix,mirrored,error),error.c_str());
    HybridInputs input{clusters,pool,page0,selection0,counters,material_buffer,arguments,128,128,1,1};
    input.view[4]=input.view[9]=input.view[14]=input.view[16]=input.view[17]=input.view[18]=1;input.view[19]=.1f;input.ambient={.65f,.75f,0,0};
    auto encoder=queue->createCommandEncoder();bool clear=true;
    for(unsigned i=0;i<3;++i) {
      input.transform=i<2?first[i]:mirrored;input.page_table=i<2?page0.get():page1.get();input.selected=i<2?selection0.get():selection1.get();
      input.material_range={i<2?0u:16*sizeof(MapRayMaterial),sizeof(MapRayMaterial)};
      check(baseline.visibility(encoder,input),"scene raster reference visibility");reference.resolve(encoder,baseline,input,clear);clear=false;
    }
    const std::array<SceneRasterAsset,2> assets{{{clusters,1,1,0,first},{clusters,1,1,16,std::span(&mirrored,1)}}};
    SceneRasterFrame frame;check(tables.begin(encoder,0,assets,frame),tables.error().c_str());
    check(tables.append_roots(encoder,0,std::span(&pages[0],1)),tables.error().c_str());
    SelectionGpuFrame selected;selected.selected=selection1;selected.counters=counters;selected.page_table=page1;
    check(tables.append(encoder,1,selected) && tables.finish(encoder),tables.error().c_str());
    HybridInputs combined{frame.clusters,pool,frame.pages,frame.selected,frame.counters,material_buffer,frame.dispatch,128,128,frame.capacity,1,input.view,input.ambient};
    combined.scene_draws=frame.draws;combined.scene_instances=frame.instances;combined.scene_frame=frame.generation;
    check(batch.visibility(encoder,combined),"scene raster accumulated visibility");observed.resolve(encoder,batch,combined,true);
    auto fence=device->createFence({});check(bool(fence),"scene raster fence");std::uint64_t signal{};
    const auto submit=[&](rhi::ICommandEncoder* commands,bool tracked) {
      auto command=commands->finish();auto* c=command.get();auto* f=fence.get();++signal;
      rhi::SubmitDesc desc{};desc.commandBuffers=&c;desc.commandBufferCount=1;desc.signalFences=&f;desc.signalFenceValues=&signal;desc.signalFenceCount=1;
      check(SLANG_SUCCEEDED(queue->submit(desc)),"scene raster submit");if(tracked)check(tables.submitted(fence,signal),tables.error().c_str());
      check(SLANG_SUCCEEDED(device->waitForFences(1,&f,&signal,true,30'000'000'000ull)),"scene raster fence deadline");
    };
    submit(encoder,true);
    std::vector<std::uint64_t> visibility(128*128);check(SLANG_SUCCEEDED(device->readBuffer(batch.visibility_buffer(),0,visibility.size()*8,visibility.data())),"scene raster visibility read");
    unsigned hits[3]{};for(const auto pixel:visibility)if(pixel) {const auto id=((~std::uint32_t(pixel)>>1)-1)/128;check(id<3,"scene raster global draw identity");++hits[id];}
    for(auto hit:hits)check(hit>100,"scene raster lost original instance");
    for(unsigned target=0;target<6;++target) {
      ComPtr<ISlangBlob> a,b;rhi::SubresourceLayout al{},bl{};
      check(SLANG_SUCCEEDED(device->readTexture(reference.colors[target],0,0,a.writeRef(),&al)) &&
          SLANG_SUCCEEDED(device->readTexture(observed.colors[target],0,0,b.writeRef(),&bl)),"scene raster material readback");
      check(al.colPitch==bl.colPitch,"scene raster format mismatch");
      for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x) {
        const auto* av=static_cast<const std::uint8_t*>(a->getBufferPointer())+y*al.rowPitch+x*al.colPitch;
        const auto* bv=static_cast<const std::uint8_t*>(b->getBufferPointer())+y*bl.rowPitch+x*bl.colPitch;
        if(target==1)for(unsigned c=0;c<4;++c) {float af,bf;std::memcpy(&af,av+c*4,4);std::memcpy(&bf,bv+c*4,4);check(std::abs(af-bf)<1e-5f,"scene raster transformed position differs");}
        else check(!std::memcmp(av,bv,al.colPitch),"scene raster authored material or normal differs");
      }
    }
    encoder=queue->createCommandEncoder();const std::uint32_t stale=0;
    check(SLANG_SUCCEEDED(encoder->uploadBufferData(frame.draws,16,4,&stale)),"scene raster stale-domain upload");
    check(batch.visibility(encoder,combined),"scene raster stale-domain visibility");submit(encoder,false);
    check(SLANG_SUCCEEDED(device->readBuffer(batch.visibility_buffer(),0,visibility.size()*8,visibility.data())),"scene raster stale-domain read");
    unsigned retained{};for(const auto pixel:visibility)if(pixel) {check(((~std::uint32_t(pixel)>>1)-1)/128!=0,"scene raster stale generation visible");++retained;}
    check(retained==hits[1]+hits[2],"stale domain invalidated independent nodes");
    std::printf("scene_raster_gpu passed=1 assets=2 original_instances=3 root_and_dag_domains=1 packed_pages=1 material_slice=4864 mirror_shear=1 single_resolve=1 stale_generation=1 physical_bytes=%llu\n",
        static_cast<unsigned long long>(bytes));return true;
  }catch(const std::exception& error) {std::fprintf(stderr,"scene_raster_gpu failed=%s\n",error.what());return false;}
}
