#include "WorldTemporal.h"
#include "RhiShader.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using namespace octaryn::client::rendering;
constexpr unsigned Size=8;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
Slang::ComPtr<rhi::ITexture> texture(rhi::IDevice* device,rhi::Format format,unsigned stride,
    const void* data=nullptr,bool output=false) {
  rhi::TextureDesc desc{};desc.size={Size,Size,1};desc.format=format;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  if(output)desc.usage|=rhi::TextureUsage::UnorderedAccess;
  if(format==rhi::Format::D32Float)desc.usage|=rhi::TextureUsage::DepthStencil;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  rhi::SubresourceData init{};init.data=data;init.rowPitch=Size*stride;init.slicePitch=Size*Size*stride;
  auto result=device->createTexture(desc,data?&init:nullptr);
  require(result!=nullptr,"temporal input texture creation");return result;
}
Slang::ComPtr<rhi::ITextureView> view(rhi::ITexture* texture) {
  Slang::ComPtr<rhi::ITextureView> result;
  require(SLANG_SUCCEEDED(texture->getDefaultView(result.writeRef())),"temporal input view creation");return result;
}
float half(std::uint16_t bits) {
  unsigned exponent=(bits>>10)&31,fraction=bits&1023;
  require(exponent!=31,"nonfinite temporal motion");
  return (bits&32768?-1.f:1.f)*std::ldexp(float(exponent?fraction+1024:fraction),exponent?int(exponent)-25:-24);
}
void execute(rhi::IDevice* device,rhi::ICommandQueue* queue,bool sky,bool object,bool reset,bool translated,
    float emission,bool forward) {
  WorldTemporal temporal;temporal.mode=1;temporal.width=temporal.height=Size;
  temporal.camera={100001,20,-100000,0,0,1.570796327f};
  auto previous=temporal.camera;if(translated)previous.x-=1;
  temporal.history.commit(previous,Size,Size);temporal.reset=reset;temporal.jitter={.375f,-.25f};
  require(create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Temporal/Inputs.slang","main",temporal.inputs),
      "production temporal pipeline creation");
  const auto projection=temporal_view(temporal.camera,Size,Size).projection;
  std::array<float,Size*Size> depths;depths.fill(sky?1.f:projection[2]-projection[3]/10.f);
  std::array<std::array<float,4>,Size*Size> objects{},opaque{},scene{},materials{};
  for(unsigned i=0;i<Size*Size;++i) {
    objects[i]=object?std::array<float,4>{.125f,-.25f,1,0}:std::array<float,4>{};
    opaque[i]=i%2?std::array<float,4>{2.5f,5.5f,999,1}:std::array<float,4>{1,.5f,.25f,1};
    scene[i]=forward?std::array<float,4>{0,0,0,1}:opaque[i];
    materials[i]={.8f,0,.04f,emission};
  }
  auto depth=texture(device,rhi::Format::D32Float,4,depths.data());
  auto color=texture(device,rhi::Format::RGBA32Float,16,scene.data(),true);auto color_view=view(color);
  auto material=texture(device,rhi::Format::RGBA32Float,16,materials.data());auto material_view=view(material);
  auto& target=temporal.targets[0];
  target.object_motion=texture(device,rhi::Format::RGBA32Float,16,objects.data());target.object_view=view(target.object_motion);
  target.opaque=texture(device,rhi::Format::RGBA32Float,16,opaque.data());target.opaque_view=view(target.opaque);
  target.motion=texture(device,rhi::Format::RG16Float,4,nullptr,true);target.motion_view=view(target.motion);
  target.reactive=texture(device,rhi::Format::R8Unorm,1,nullptr,true);target.reactive_view=view(target.reactive);
  auto commands=queue->createCommandEncoder();require(commands!=nullptr,"temporal encoder creation");
  require(prepare_temporal(temporal,commands,0,depth,color_view,material_view),"production temporal input dispatch");
  auto submission=commands->finish();require(submission!=nullptr && SLANG_SUCCEEDED(queue->submit(submission)),"temporal submission");
  require(SLANG_SUCCEEDED(queue->waitOnHost()),"temporal completion");
  Slang::ComPtr<ISlangBlob> motion,reactive,mapped;rhi::SubresourceLayout motion_layout{},reactive_layout{},mapped_layout{};
  require(SLANG_SUCCEEDED(device->readTexture(target.motion,0,0,motion.writeRef(),&motion_layout)),"temporal motion readback");
  require(SLANG_SUCCEEDED(device->readTexture(target.reactive,0,0,reactive.writeRef(),&reactive_layout)),"temporal mask readback");
  require(SLANG_SUCCEEDED(device->readTexture(color,0,0,mapped.writeRef(),&mapped_layout)),"temporal tone-map readback");
  const float expected_x=reset?0:object?.125f:translated&&!sky?.05f:0;
  const float expected_y=!reset&&object?-.25f:0;
  const float expected_mask=forward?.9f:std::min(.9f,emission);
  for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x) {
    const auto* pixel=reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(motion->getBufferPointer())+
        y*motion_layout.rowPitch+x*motion_layout.colPitch);
    require(std::abs(half(pixel[0])-expected_x)<.0002f && std::abs(half(pixel[1])-expected_y)<.0002f,
        "actual camera/object motion or jitter convention incorrect");
    const auto mask=*(static_cast<const unsigned char*>(reactive->getBufferPointer())+y*reactive_layout.rowPitch+x*reactive_layout.colPitch);
    require(std::abs(float(mask)/255-expected_mask)<.0041f,"actual reactive mask clamp incorrect");
    const auto* output=reinterpret_cast<const float*>(static_cast<const char*>(mapped->getBufferPointer())+
        y*mapped_layout.rowPitch+x*mapped_layout.colPitch);
    for(unsigned c=0;c<3;++c) {
      const float value=scene[y*Size+x][c];
      require(std::abs(output[c]-value/(1+value))<.000001f,"actual presentation-domain conversion incorrect");
    }
  }
}
}
void qualify_temporal_inputs(rhi::IDevice* device,rhi::ICommandQueue* queue) {
  execute(device,queue,false,false,false,false,0,false);
  execute(device,queue,false,false,false,true,0,false);
  execute(device,queue,true,false,false,true,0,false);
  execute(device,queue,false,true,false,true,.4f,false);
  execute(device,queue,false,true,true,true,0,true);
  std::puts("temporal_inputs=passed cases=5 pixels=320 depth=D32 motion=RG16 jitter=excluded camera=analytic objects=override reactive=bounded tone_map=analytic");
}
