#include "WorldTemporal.h"
#include "RhiShader.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace octaryn::client::rendering {
void configure_temporal(WorldTemporal& t,const WorldSceneSettings& s,bool accept_mode) {
  if(accept_mode)t.requested_mode=std::min(s.upscaler_mode,6u);
  const float custom=temporal_scale(s.fsr_render_scale,.667f);
  const float low=temporal_scale(s.fsr_min_scale,.5f),high=std::max(low,temporal_scale(s.fsr_max_scale,1));
  const unsigned fps=std::clamp(s.fsr_target_fps,30u,240u);
  if(t.dynamic_requested!=s.fsr_dynamic_resolution || t.minimum_scale!=low || t.maximum_scale!=high ||
      (t.requested_mode==6 && t.custom_scale!=custom))t.reconfigure=true;
  if(t.target_fps!=fps) {t.resolution.target_fps=fps;t.resolution.samples=0;t.resolution.average_ms=0;}
  t.custom_scale=custom;t.minimum_scale=low;t.maximum_scale=high;t.target_fps=fps;
  t.dynamic_requested=s.fsr_dynamic_resolution;t.sharpening=s.fsr_sharpening;
  t.sharpness=std::isfinite(s.fsr_sharpness)?std::clamp(s.fsr_sharpness,0.f,1.f):.2f;
}
void update_temporal_size(WorldTemporal& t) {
  t.width=std::max(1u,unsigned(std::round(float(t.display_width)*t.resolution.scale)));
  t.height=std::max(1u,unsigned(std::round(float(t.display_height)*t.resolution.scale)));
}
bool temporal_mode(WorldTemporal& t,const char* name) {
  constexpr const char* names[]={"off","native","quality","balanced","performance","ultra-performance","custom"};
  if(!name || !*name)name="off";
  for(unsigned i=0;i<std::size(names);++i)if(!std::strcmp(name,names[i])) {t.mode=i;t.requested_mode=i;return true;}
  std::fputs("OCTARYN_CLIENT_UPSCALER requires off, native, quality, balanced, performance, ultra-performance or custom\n",stderr);
  return false;
}
namespace {
bool texture(rhi::IDevice* device,unsigned width,unsigned height,rhi::Format format,rhi::TextureUsage usage,
    const char* label,Slang::ComPtr<rhi::ITexture>& texture,Slang::ComPtr<rhi::ITextureView>& view) {
  view.setNull();texture.setNull();rhi::TextureDesc desc{};
  desc.size={width,height,1};desc.format=format;desc.label=label;
  desc.usage=usage|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination|rhi::TextureUsage::CopySource;desc.defaultState=rhi::ResourceState::ShaderResource;
  return SLANG_SUCCEEDED(device->createTexture(desc,nullptr,texture.writeRef())) &&
      SLANG_SUCCEEDED(texture->getDefaultView(view.writeRef()));
}
}
bool resize_temporal(WorldTemporal& t,rhi::IDevice* device,unsigned width,unsigned height,unsigned slots) {
  destroy_fsr2(t.fsr);t.fsr=nullptr;t.history.invalidate();t.last={};
  t.targets={};
  t.display_width=width;t.display_height=height;
  t.timing.reset();
  const bool dynamic=t.dynamic_requested && t.mode>=2 && t.timing.initialize(device);
  if(t.dynamic_requested && t.mode>=2 && !dynamic)
    std::fputs("FSR dynamic resolution unavailable: GPU timestamps not supported\n",stderr);
  t.resolution.configure(t.mode,t.custom_scale,dynamic,t.minimum_scale,t.maximum_scale,t.target_fps);
  update_temporal_size(t);
  const float allocation_scale=dynamic?t.resolution.maximum:t.resolution.scale;
  t.allocation_width=std::max(1u,unsigned(std::round(float(width)*allocation_scale)));
  t.allocation_height=std::max(1u,unsigned(std::round(float(height)*allocation_scale)));
  t.reconfigure=false;
  if(!t.mode)return true;
  if(!t.inputs && !create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Temporal/Inputs.slang","main",t.inputs))return false;
  t.fsr=create_fsr2(device,{t.allocation_width,t.allocation_height,width,height,false,false,false,dynamic});
  if(!t.fsr)return false;
  for(unsigned slot=0;slot<slots;++slot) {
    auto& f=t.targets[slot];
    if(!texture(device,t.allocation_width,t.allocation_height,rhi::Format::RGBA16Float,rhi::TextureUsage::RenderTarget,
        "temporal_opaque",f.opaque,f.opaque_view) ||
       !texture(device,t.allocation_width,t.allocation_height,rhi::Format::RGBA16Float,rhi::TextureUsage::RenderTarget,
        "temporal_object_motion",f.object_motion,f.object_view) ||
       !texture(device,t.allocation_width,t.allocation_height,rhi::Format::RG16Float,rhi::TextureUsage::UnorderedAccess,
        "temporal_motion",f.motion,f.motion_view) ||
       !texture(device,t.allocation_width,t.allocation_height,rhi::Format::R8Unorm,rhi::TextureUsage::UnorderedAccess,
        "temporal_reactive",f.reactive,f.reactive_view) ||
       !texture(device,width,height,rhi::Format::RGBA16Float,rhi::TextureUsage::UnorderedAccess,
        "temporal_output",f.output,f.output_view))return false;
  }
  std::printf("world_fsr2 version=2.2.1 mode=%u render=%ux%u output=%ux%u domain=tone_mapped_linear\n",t.mode,t.width,t.height,width,height);
  return true;
}
}
