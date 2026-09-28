#include "HybridRenderer.h"
#include "GeometryFormat.h"
#include "PageResidency.h"
#include "MapRendererInternal.h"
#include "WorldHdr.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <vector>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
namespace {
void check(bool value,const char* error){if(!value)throw std::runtime_error(error);}
ComPtr<rhi::IBuffer> make_buffer(rhi::IDevice* device,const void* data,size_t bytes,unsigned stride,bool indirect=false) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  if(indirect)desc.usage|=rhi::BufferUsage::IndirectArgument;
  ComPtr<rhi::IBuffer> buffer;check(SLANG_SUCCEEDED(device->createBuffer(desc,data,buffer.writeRef())),"hybrid buffer");return buffer;
}
void finish(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder) {
  auto command=encoder->finish();check(bool(command),"hybrid commands");
  auto fence=device->createFence({});check(bool(fence),"hybrid fence");
  auto* c=command.get();auto* f=fence.get();uint64_t value=1;
  rhi::SubmitDesc submit{};submit.commandBuffers=&c;submit.commandBufferCount=1;
  submit.signalFences=&f;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  check(SLANG_SUCCEEDED(queue->submit(submit)),"hybrid submit");
  check(SLANG_SUCCEEDED(device->waitForFences(1,&f,&value,true,30'000'000'000ull)),"hybrid timeout");
}
}
bool probe_hybrid_geometry(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* directory) {
  try {
    constexpr unsigned extent=128;
    HybridRenderer renderer;
    check(renderer.initialize(device,directory,world_gbuffer_formats,rhi::Format::D32Float),"hybrid pipeline initialization");
    check(renderer.resize(device,extent,extent),"hybrid target allocation");
    std::array<GeometryCluster,3> clusters{};std::vector<uint8_t> pool(3*page_bytes);
    // A large opaque triangle, a tiny opaque triangle, and a fully discarded alpha mask.
    const float xy[3][6]={{-.7f,-.7f,0,.7f,.7f,-.7f},{-.95f,.90f,-.93f,.94f,-.91f,.90f},{.75f,.75f,.85f,.95f,.95f,.75f}};
    for(unsigned i=0;i<3;++i) {
      auto& cluster=clusters[i];cluster.page=i;cluster.material=i;cluster.vertex_count=3;
      cluster.triangle_count=1;cluster.triangle_offset=3*sizeof(MapVertex);cluster.flags=256|(i==2?1:0);
      MapVertex vertices[3]{};
      for(unsigned j=0;j<3;++j) {
        vertices[j].position[0]=xy[i][j*2];vertices[j].position[1]=xy[i][j*2+1];vertices[j].position[2]=1;
        vertices[j].normal[2]=-1;vertices[j].tangent[0]=vertices[j].tangent[3]=1;
      }
      std::memcpy(pool.data()+i*page_bytes,vertices,sizeof(vertices));
      const uint32_t triangle=0x020100;std::memcpy(pool.data()+i*page_bytes+cluster.triangle_offset,&triangle,4);
    }
    std::array<GpuPage,3> pageTable{{{0,1,1,0},{1,1,1,0},{2,1,1,0}}};
    const uint32_t selected[12]={0,0,0,1,1,1,1,1,2,2,2,1},counters[5]={3,0,0,0,0},arguments[6]={1,1,1,3,1,1};
    MapRayMaterial materials[3]{};for(auto& material:materials){material.double_sided=1;material.base_color[0]=.8f;}
    materials[2].alpha_mode=1;materials[2].alpha_cutoff=.5f;materials[2].base_color[3]=0;
    auto clusterBuffer=make_buffer(device,clusters.data(),sizeof(clusters),sizeof(GeometryCluster));
    auto poolBuffer=make_buffer(device,pool.data(),pool.size(),0);
    auto pages=make_buffer(device,pageTable.data(),sizeof(pageTable),sizeof(GpuPage));
    auto selection=make_buffer(device,selected,sizeof(selected),16);
    auto counterBuffer=make_buffer(device,counters,sizeof(counters),4);
    auto dispatch=make_buffer(device,arguments,sizeof(arguments),4,true);
    auto materialBuffer=make_buffer(device,materials,sizeof(materials),sizeof(MapRayMaterial));
    HybridInputs input{clusterBuffer,poolBuffer,pages,selection,counterBuffer,materialBuffer,dispatch,extent,extent,3,3};
    input.view[4]=1;input.view[9]=1;input.view[14]=1;input.view[16]=input.view[17]=input.view[18]=1;input.view[19]=.1f;
    input.ambient={.65f,.75f,0,0};
    std::array<ComPtr<rhi::ITexture>,6> textures;std::array<ComPtr<rhi::ITextureView>,6> views;
    std::array<rhi::RenderPassColorAttachment,6> attachments;
    for(unsigned i=0;i<6;++i) {
      rhi::TextureDesc desc{};desc.size={extent,extent,1};desc.format=world_gbuffer_formats[i];
      desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;desc.defaultState=rhi::ResourceState::RenderTarget;
      textures[i]=device->createTexture(desc);check(bool(textures[i]),"hybrid color target");
      views[i]=textures[i]->getDefaultView();attachments[i].view=views[i];
    }
    rhi::TextureDesc depthDesc{};depthDesc.size={extent,extent,1};depthDesc.format=rhi::Format::D32Float;
    depthDesc.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::CopySource;depthDesc.defaultState=rhi::ResourceState::DepthWrite;
    auto depth=device->createTexture(depthDesc);check(bool(depth),"hybrid depth target");auto depthView=depth->getDefaultView();
    rhi::RenderPassDepthStencilAttachment depthAttachment{};depthAttachment.view=depthView;
    auto encoder=queue->createCommandEncoder();check(renderer.visibility(encoder,input),"hybrid visibility recording");
    rhi::RenderPassDesc render{};render.colorAttachments=attachments.data();render.colorAttachmentCount=6;
    render.depthStencilAttachment=&depthAttachment;auto* pass=encoder->beginRenderPass(render);
    check(renderer.resolve(pass,input),"hybrid resolve recording");pass->end();finish(device,queue,encoder);
    std::vector<uint64_t> pixels(extent*extent);
    check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,pixels.size()*8,pixels.data())),"hybrid visibility readback");
    unsigned hits[3]{};
    for(auto key:pixels)if(key) {
      const auto id=(~uint32_t(key)>>1)-1;check(id/128<3,"invalid hybrid visibility ID");++hits[id/128];
      float z;uint32_t bits=~uint32_t(key>>32);std::memcpy(&z,&bits,4);
      check(z>.8999f && z<.9001f,"hybrid depth reconstruction");
    }
    check(hits[0]>3000,"hardware raster lost large triangle");
    check(hits[1]>0 && hits[1]<12,"compute raster lost tiny triangle");
    check(hits[2]==0,"alpha mask committed visibility");
    Slang::ComPtr<ISlangBlob> depthPixels;rhi::SubresourceLayout depthLayout{};
    check(SLANG_SUCCEEDED(device->readTexture(depth,0,0,depthPixels.writeRef(),&depthLayout)),"hybrid exported depth readback");
    for(unsigned y=0;y<extent;++y)for(unsigned x=0;x<extent;++x) {
      float value;std::memcpy(&value,static_cast<const uint8_t*>(depthPixels->getBufferPointer())+
          y*depthLayout.rowPitch+x*depthLayout.colPitch,4);
      check(pixels[y*extent+x]?(value>.8999f && value<.9001f):value==1,"material resolve depth does not match visibility");
    }
    // A stale page-generation reference must disappear, even if its slot is reused.
    pageTable[1].generation=2;encoder=queue->createCommandEncoder();
    check(SLANG_SUCCEEDED(encoder->uploadBufferData(pages,0,sizeof(pageTable),pageTable.data())),"hybrid page generation upload");
    check(renderer.visibility(encoder,input),"hybrid stale page recording");finish(device,queue,encoder);
    check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,pixels.size()*8,pixels.data())),"hybrid stale readback");
    for(auto key:pixels)if(key)check(((~uint32_t(key)>>1)-1)/128!=1,"stale page still rasterized");
    // Force the same tiny triangle through hardware with a mask-class flag,
    // while its actual material remains opaque. Compare both winding orders.
    pageTable[1].generation=1;
    const auto coverage=[&](bool hardware,bool reversed,bool twoSided) {
      clusters[1].flags=(hardware?1u:0u)|(twoSided?256u:0u);
      const uint32_t tri=reversed?0x010200:0x020100;
      auto commands=queue->createCommandEncoder();
      check(SLANG_SUCCEEDED(commands->uploadBufferData(pages,0,sizeof(pageTable),pageTable.data())),"coverage pages");
      check(SLANG_SUCCEEDED(commands->uploadBufferData(clusterBuffer,0,sizeof(clusters),clusters.data())),"coverage clusters");
      check(SLANG_SUCCEEDED(commands->uploadBufferData(poolBuffer,page_bytes+clusters[1].triangle_offset,4,&tri)),"coverage winding");
      check(renderer.visibility(commands,input),"coverage raster");finish(device,queue,commands);
      check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,pixels.size()*8,pixels.data())),"coverage readback");
      std::vector<unsigned> covered;
      for(unsigned p=0;p<pixels.size();++p)if(pixels[p] && ((~uint32_t(pixels[p])>>1)-1)/128==1)covered.push_back(p);
      return covered;
    };
    for(bool twoSided:{false,true})for(bool reversed:{false,true}) {
      const auto hardware=coverage(true,reversed,twoSided),software=coverage(false,reversed,twoSided);
      if(hardware!=software)std::fprintf(stderr,"coverage_mismatch two_sided=%d reversed=%d hardware=%zu software=%zu\n",twoSided,reversed,hardware.size(),software.size());
      check(hardware==software,"compute/hardware winding or coverage disagreement");
      if(twoSided)check(!hardware.empty(),"double-sided triangle missing");
    }
    const uint32_t phases[3]={1,3,1};
    auto flags=make_buffer(device,phases,sizeof(phases),4);
    input.occlusion_flags=flags;input.occlusion_phase=1;
    encoder=queue->createCommandEncoder();
    check(renderer.visibility(encoder,input),"phase-one raster");finish(device,queue,encoder);
    check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,pixels.size()*8,pixels.data())),"phase-one readback");
    unsigned firstHits{};
    for(auto key:pixels)if(key) {check(((~uint32_t(key)>>1)-1)/128==0,"phase-one included deferred triangle");++firstHits;}
    check(firstHits==hits[0],"phase-one changed hardware coverage");
    input.occlusion_phase=3;encoder=queue->createCommandEncoder();
    check(renderer.visibility(encoder,input,false),"phase-two accumulate");finish(device,queue,encoder);
    check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,pixels.size()*8,pixels.data())),"phase-two readback");
    unsigned finalHits[3]{};for(auto key:pixels)if(key)++finalHits[((~uint32_t(key)>>1)-1)/128];
    check(finalHits[0]==hits[0] && finalHits[1]==hits[1],"late raster lost first-phase visibility");
    const uint32_t shuffled[12]={1,1,1,1,2,2,2,1,0,0,0,1};
    input.occlusion_phase=0;encoder=queue->createCommandEncoder();
    check(SLANG_SUCCEEDED(encoder->uploadBufferData(selection,0,sizeof(shuffled),shuffled)),"shuffle selection");
    check(renderer.visibility(encoder,input),"stable identity raster");finish(device,queue,encoder);
    std::vector<uint64_t> shuffledPixels(pixels.size());
    check(SLANG_SUCCEEDED(device->readBuffer(renderer.visibility_buffer(),0,shuffledPixels.size()*8,shuffledPixels.data())),"stable identity readback");
    for(unsigned p=0;p<pixels.size();++p)check(uint32_t(pixels[p])==uint32_t(shuffledPixels[p]),"visibility identity depends on compaction order");
    std::printf("geometry_hybrid_gpu passed=1 hardware_pixels=%u compute_pixels=%u masked_pixels=%u stale_generation=1\n",hits[0],hits[1],hits[2]);
    return true;
  }catch(const std::exception& error){std::fprintf(stderr,"geometry_hybrid_gpu failed=%s\n",error.what());return false;}
}
