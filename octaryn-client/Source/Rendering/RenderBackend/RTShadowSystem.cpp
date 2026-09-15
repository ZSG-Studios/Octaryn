#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
namespace {
bool resize(WorldRenderer& r) {
  auto& s=r.rt_shadows;
  const unsigned width=r.temporal.allocation_width,height=r.temporal.allocation_height;
  if(s.width==width && s.height==height)return true;
  // Allocation-size changes occur only through the existing drained resize path.
  s.width=width;s.height=height;s.valid=false;
  for(auto& h:s.history) {
    auto texture=[&](rhi::Format format,auto& t,auto& view) {
      view.setNull();t.setNull();rhi::TextureDesc d{};d.size={width,height,1};d.format=format;
      d.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopyDestination;
      d.defaultState=rhi::ResourceState::ShaderResource;
      return world_rhi_ok(r.device->createTexture(d,nullptr,t.writeRef())) && world_rhi_ok(t->getDefaultView(view.writeRef()));
    };
    if(!texture(rhi::Format::R32Float,h.raw,h.raw_view) || !texture(rhi::Format::RGBA32Float,h.shadow,h.shadow_view) ||
       !texture(rhi::Format::RGBA32Float,h.position,h.position_view) || !texture(rhi::Format::RGBA8Unorm,h.voxel,h.voxel_view))return false;
  }
  return true;
}
}
bool initialize_rt_shadows(WorldRenderer& r) {
  if(!world_ray_available(r))return true;
  return create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/RayTracing/Shadow.slang","main",r.rt_shadows.trace) &&
    create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Shadows/Temporal.slang","main",r.rt_shadows.filter);
}
bool update_rt_shadows(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.rt_shadows;auto& hdr=r.target().hdr;
  hdr.ray_shadows=false;s.rays=0;
  if(!r.ray_enabled || !world_ray_available(r) || !s.trace)return true;
  if(!resize(r))return false;
  auto& current=s.history[s.index];auto& previous=s.history[1-s.index];
  const unsigned extent[2]={unsigned(r.render_width()),unsigned(r.render_height())};
  const float eye[4]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
  const float sun[4]={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
  float camera_delta=0;for(unsigned i=0;i<3;++i) {const float d=eye[i]-s.previous_view[i];camera_delta+=d*d;}
  const bool valid=s.valid && !(r.temporal.mode && r.temporal.reset) && camera_delta<64 && s.revision==r.scene_changes.revision() &&
    s.active_width==extent[0] && s.active_height==extent[1] && s.range==r.lighting_settings.shadow_distance &&
    sun[0]*s.sun[0]+sun[1]*s.sun[1]+sun[2]*s.sun[2]>.9999f;
  if(!s.valid) {
    float zero[4]{};
    for(auto& h:s.history)for(auto* texture:{h.shadow.get(),h.position.get(),h.voxel.get()}) {
      commands->clearTextureFloat(texture,{0,1,0,1},zero);
      commands->setTextureState(texture,rhi::ResourceState::ShaderResource);
    }
  }
  r.lighting_profile.begin_pass(commands,LightingPass::SunTrace);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(s.trace);
  bool ok=root && world_ray_bind(r,root) && bind_world_atlas(r.atlas,root);
  const float sampling[2]={r.lighting_settings.sun_angular_radius,float(r.frames%4096)};
  if(ok) {
    rhi::ShaderCursor c(root);
    ok=world_rhi_ok(c["positions"].setBinding(hdr.views[1])) && world_rhi_ok(c["voxels"].setBinding(hdr.views[2])) &&
      world_rhi_ok(c["visibility"].setBinding(current.raw_view)) && world_rhi_ok(c["eye"].setData(eye,sizeof(eye))) &&
      world_rhi_ok(c["sun"].setData(sun,sizeof(sun))) && world_rhi_ok(c["extent"].setData(extent,sizeof(extent))) &&
      world_rhi_ok(c["sampling"].setData(sampling,sizeof(sampling))) &&
      world_rhi_ok(c["shadowRange"].setData(&r.lighting_settings.shadow_distance,sizeof(float)));
  }
  if(ok)pass->dispatchCompute((extent[0]+7)/8,(extent[1]+7)/8,1);
  pass->end();if(!ok)return false;
  r.lighting_profile.mark(commands,LightingPass::SunTrace);
  commands->setTextureState(current.raw,rhi::ResourceState::ShaderResource);
  r.lighting_profile.begin_pass(commands,LightingPass::SunFilter);
  pass=commands->beginComputePass();if(!pass)return false;
  root=pass->bindPipeline(s.filter);ok=root!=nullptr;
  if(ok) {
    rhi::ShaderCursor c(root);
    const float options[4]={float(extent[0]),float(extent[1]),valid?1.f:0.f,r.lighting_settings.shadow_history_weight};
    ok=world_rhi_ok(c["currentShadow"].setBinding(current.raw_view)) && world_rhi_ok(c["positions"].setBinding(hdr.views[1])) &&
      world_rhi_ok(c["voxels"].setBinding(hdr.views[2])) && world_rhi_ok(c["previousShadow"].setBinding(previous.shadow_view)) &&
      world_rhi_ok(c["previousPositions"].setBinding(previous.position_view)) && world_rhi_ok(c["previousVoxels"].setBinding(previous.voxel_view)) &&
      world_rhi_ok(c["shadowHistory"].setBinding(current.shadow_view)) && world_rhi_ok(c["positionHistory"].setBinding(current.position_view)) &&
      world_rhi_ok(c["voxelHistory"].setBinding(current.voxel_view)) && world_rhi_ok(c["outputShadow"].setBinding(hdr.sun_visibility_view)) &&
      world_rhi_ok(c["eye"].setData(eye,sizeof(eye))) && world_rhi_ok(c["options"].setData(options,sizeof(options)));
    const char* names[]={"previousEye","previousRight","previousUp","previousForward","previousProjection"};
    for(unsigned i=0;ok && i<5;++i)ok=world_rhi_ok(c[names[i]].setData(s.previous_view.data()+i*4,16));
  }
  if(ok)pass->dispatchCompute((extent[0]+7)/8,(extent[1]+7)/8,1);
  pass->end();if(!ok)return false;
  r.lighting_profile.mark(commands,LightingPass::SunFilter);
  for(auto* t:{current.shadow.get(),current.position.get(),current.voxel.get(),hdr.sun_visibility.get()})
    commands->setTextureState(t,rhi::ResourceState::ShaderResource);
  std::copy_n(r.draw_uniforms.begin(),20,s.previous_view.begin());
  std::copy_n(sun,3,s.sun.begin());s.revision=r.scene_changes.revision();s.valid=true;s.index=1-s.index;
  s.range=r.lighting_settings.shadow_distance;
  s.active_width=extent[0];s.active_height=extent[1];
  s.rays=std::uint64_t(extent[0])*extent[1];hdr.ray_shadows=true;return true;
}
}
