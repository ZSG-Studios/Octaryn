#include "WorldHdr.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include "RhiShader.h"
namespace octaryn::client::rendering {
bool create_world_hdr(rhi::IDevice* device,WorldHdr& hdr) {
  return create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Composite.slang","main",hdr.composite) &&
      (!device->hasFeature(rhi::Feature::RayQuery) ||
       create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/CompositeRT.slang","main",hdr.composite_rt)) &&
      create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/CompositeSrc.slang","main",hdr.composite_src) &&
      create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Present.slang","main",hdr.present);
}
bool resize_world_hdr(rhi::IDevice* device,WorldHdr& hdr,unsigned width,unsigned height) {
  rhi::TextureDesc desc{};desc.size={width,height,1};
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::ShaderResource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  for(unsigned i=0;i<4;++i) {
    hdr.views[i].setNull();hdr.gbuffer[i].setNull();desc.format=world_gbuffer_formats[i];
    if(SLANG_FAILED(device->createTexture(desc,nullptr,hdr.gbuffer[i].writeRef())) ||
       SLANG_FAILED(hdr.gbuffer[i]->getDefaultView(hdr.views[i].writeRef()))) return false;
  }
  hdr.scene_view.setNull();hdr.scene.setNull();desc.format=rhi::Format::RGBA16Float;
  desc.usage|=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  if(SLANG_FAILED(device->createTexture(desc,nullptr,hdr.scene.writeRef())) ||
      SLANG_FAILED(hdr.scene->getDefaultView(hdr.scene_view.writeRef())))return false;
  hdr.sun_visibility_view.setNull();hdr.sun_visibility.setNull();hdr.ray_shadows=false;
  desc.format=rhi::Format::R32Float;desc.label="sun_visibility";
  desc.usage=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
  return SLANG_SUCCEEDED(device->createTexture(desc,nullptr,hdr.sun_visibility.writeRef())) &&
      SLANG_SUCCEEDED(hdr.sun_visibility->getDefaultView(hdr.sun_visibility_view.writeRef()));
}
bool composite_world_hdr(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& hdr=r.target().hdr;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  const bool raySky=r.ray_enabled && world_ray_available(r) && hdr.composite_rt;
  const bool srcSky=r.src_enabled && r.src.initialized && r.src.active && hdr.composite_src;
  auto* root=pass->bindPipeline(srcSky?hdr.composite_src:raySky?hdr.composite_rt:hdr.composite);bool ok=root!=nullptr;
  if(ok && srcSky)ok=world_src_bind(r,root);
  if(ok && raySky && !srcSky)ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root);
  if(ok) {
    rhi::ShaderCursor c(root);
    const float lighting[4]={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],r.fog_distance};
    const float sun[4]={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
    const float dimensions[4]={float(r.render_width()),float(r.render_height()),hdr.ray_shadows?1.f:0.f,float(r.lighting_settings.debug_view)};
    const float eye[4]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
    const char* names[]={"colors","positions","voxels","materials"};
    for(unsigned i=0;ok && i<4;++i)ok=world_rhi_ok(c[names[i]].setBinding(hdr.views[i]));
    ok=ok && world_rhi_ok(c["lighting"].setData(lighting,sizeof(lighting))) && world_rhi_ok(c["sun"].setData(sun,sizeof(sun))) &&
      world_rhi_ok(c["dimensions"].setData(dimensions,sizeof(dimensions))) && world_rhi_ok(c["eye"].setData(eye,sizeof(eye))) &&
      world_rhi_ok(c["scene"].setBinding(hdr.scene_view)) && world_rhi_ok(c["sunVisibility"].setBinding(hdr.sun_visibility_view)) &&
      world_rhi_ok(c["sunHistory"].setBinding(r.rt_shadows.valid?r.rt_shadows.history[1-r.rt_shadows.index].shadow_view.get():hdr.sun_visibility_view.get())) &&
      world_ddgi_bind(r,root) && world_local_lighting_bind(r,root);
  }
  if(ok)pass->dispatchCompute(unsigned(r.render_width()+7)/8,unsigned(r.render_height()+7)/8,1);
  pass->end();return ok;
}
bool present_world_hdr(rhi::ICommandEncoder* commands,WorldHdr& hdr,rhi::ITextureView* output,unsigned width,unsigned height,rhi::ITextureView* scene) {
  auto* pass=commands->beginComputePass();if(!pass) return false;
  auto* root=pass->bindPipeline(hdr.present);
  const unsigned options[4]={scene?1u:0u,0,0,0};
  bool ok=root && SLANG_SUCCEEDED(root->setBinding({0,0,0},rhi::Binding(scene?scene:hdr.scene_view.get()))) &&
      SLANG_SUCCEEDED(root->setData({0,0,0},options,sizeof(options))) &&
      SLANG_SUCCEEDED(root->setBinding({0,1,0},rhi::Binding(output)));
  if(ok) pass->dispatchCompute((width+7)/8,(height+7)/8,1);
  pass->end();return ok;
}
}
