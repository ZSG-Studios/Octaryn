#include "SceneEnvironmentBinding.h"
#include "WorldHdr.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include "RhiShader.h"
namespace octaryn::client::rendering {
bool create_world_hdr(rhi::IDevice* device,WorldHdr& hdr) {
  hdr.attachment_count=world_gbuffer_attachment_count(device);
  return create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Composite.slang","main",hdr.composite) &&
      (!device->hasFeature(rhi::Feature::RayQuery) ||
       create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/CompositeRT.slang","main",hdr.composite_rt)) &&
      create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Present.slang","main",hdr.present);
}
bool resize_world_hdr(rhi::IDevice* device,WorldHdr& hdr,unsigned width,unsigned height) {
  rhi::TextureDesc desc{};desc.size={width,height,1};
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  for(unsigned i=0;i<world_gbuffer_formats.size();++i) {
    hdr.views[i].setNull();hdr.gbuffer[i].setNull();
    if(i>=hdr.attachment_count)continue;
    desc.format=world_gbuffer_formats[i];
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
  if(r.gpu_counters)r.gpu_counters->begin(commands,r.frames,r.map && r.map_reflections.enabled &&
      r.ray_effects && r.ray_enabled && r.lighting_settings.reflection_distance>0 &&
      (!r.tile_session || r.tile_session->capture_ready()) && world_ray_coverage_complete(r),r.scene_changes.revision());
  const bool reflections_ok=render_map_reflections(r,commands);
  if(r.gpu_counters) {
    const auto& t=r.temporal;const auto& c=t.camera;
    r.gpu_counters->end(commands,{{c.x,c.y,c.z,c.yaw,c.pitch,c.vertical_fov},
        {r.render_width(),r.render_height(),r.width,r.height},t.validation_frame,t.sampling_frame,
        t.reflection_sampling_frame,t.delta_ms,t.fixed_sampling});
  }
  if(!reflections_ok)return false;
  r.lighting_profile.begin_pass(commands,LightingPass::Composition);
  auto& hdr=r.target().hdr;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  const bool raySky=r.ray_effects && r.ray_enabled && world_ray_available(r) && hdr.composite_rt;
  auto* root=pass->bindPipeline(raySky?hdr.composite_rt:hdr.composite);bool ok=root && bind_scene_environment(r,root);
  if(ok && raySky)ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root);
  if(ok && raySky) {
    rhi::ShaderCursor c(root);
    const auto& reflections=r.map_reflections;
    const unsigned enabled=reflections.pending?1u:0u;
    auto* reflected=enabled?(reflections.reference?reflections.history[reflections.pending_index].radiance.view.get():reflections.filtered.view.get()):hdr.views[0].get();
    auto* reflected_depth=enabled?reflections.history[reflections.pending_index].position.view.get():hdr.views[1].get();
    auto* reflected_surface=enabled?reflections.history[reflections.pending_index].surface.view.get():hdr.views[2].get();
    ok=world_rhi_ok(c["useTemporalReflections"].setData(&enabled,sizeof(enabled))) &&
        world_rhi_ok(c["temporalReflections"].setBinding(reflected)) &&
        world_rhi_ok(c["temporalReflectionDepth"].setBinding(reflected_depth)) &&
        world_rhi_ok(c["temporalReflectionSurface"].setBinding(reflected_surface));
    const float reflection_dimensions[4]={float(reflections.width?reflections.width:r.render_width()),
        float(reflections.height?reflections.height:r.render_height()),0,0};
    ok=ok && world_rhi_ok(c["reflectionDimensions"].setData(reflection_dimensions,sizeof(reflection_dimensions)));
  }
  if(ok) {
    rhi::ShaderCursor c(root);
    const float lighting[4]={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],r.scene_environment.enabled && !r.scene_environment.sky_enabled?0.f:r.fog_distance};
    const auto sun=scene_sun(r);
    const float dimensions[4]={float(r.render_width()),float(r.render_height()),hdr.ray_shadows?1.f:0.f,float(r.lighting_settings.debug_view)};
    const float eye[4]={r.view_uniforms[0],r.view_uniforms[1],r.view_uniforms[2],0};
    const char* names[]={"colors","positions","voxels","materials","emissive"};
    for(unsigned i=0;ok && i<5;++i)ok=world_rhi_ok(c[names[i]].setBinding(hdr.views[i]));
    if(c["blockSurfaceKeys"].isValid())ok=ok && world_rhi_ok(c["blockSurfaceKeys"].setBinding(hdr.views[5]));
    ok=ok && world_rhi_ok(c["lighting"].setData(lighting,sizeof(lighting))) && world_rhi_ok(c["sun"].setData(sun.data(),sizeof(sun))) &&
      world_rhi_ok(c["dimensions"].setData(dimensions,sizeof(dimensions))) && world_rhi_ok(c["eye"].setData(eye,sizeof(eye))) &&
      world_rhi_ok(c["scene"].setBinding(hdr.scene_view)) && world_rhi_ok(c["sunVisibility"].setBinding(hdr.sun_visibility_view)) &&
      world_rhi_ok(c["sunHistory"].setBinding(r.rt_shadows.valid?r.rt_shadows.history[1-r.rt_shadows.index].shadow_view.get():hdr.sun_visibility_view.get())) &&
      bind_block_transport_lookup(r,root) && world_local_lighting_bind(r,root);
  }
  if(ok)pass->dispatchCompute(unsigned(r.render_width()+7)/8,unsigned(r.render_height()+7)/8,1);
  pass->end();
  r.lighting_profile.mark(commands,LightingPass::Composition);
  return ok;
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
