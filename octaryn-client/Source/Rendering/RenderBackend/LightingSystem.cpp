#include "WorldRendererInternal.h"
#include "LightingSystem.h"
#include "LightingGraph.h"
#include <cstdlib>
#include <string_view>
namespace octaryn::client::rendering {
namespace {
void apply_quality(WorldRenderer& r) {
  const auto quality=r.lighting_settings.quality;
  r.restir.settings.candidates=quality==LightingQuality::Low?4:quality==LightingQuality::Ultra?16:8;
  r.restir.settings.spatial=quality!=LightingQuality::Low;
  r.restir.settings.spatial_samples=quality==LightingQuality::Ultra?8:4;
  r.restir.settings.debug=r.lighting_settings.debug_view>=13?r.lighting_settings.debug_view-12:0;
}
}
bool open_world_renderer_set_lighting_options(WorldRenderer* r,const LightingSettings& settings) {
  if(!r || settings.quality>LightingQuality::Ultra || !std::isfinite(settings.sun_angular_radius) ||
     !std::isfinite(settings.shadow_history_weight) || settings.sun_angular_radius<0 || settings.sun_angular_radius>.1f ||
     settings.shadow_history_weight<0 || settings.shadow_history_weight>=1 || settings.shadow_resolution<256 ||
     settings.shadow_resolution>2048 || settings.debug_view>20)return false;
  if(settings.shadow_resolution!=r->lighting_settings.shadow_resolution && !open_world_renderer_flush(r))return false;
  r->lighting_settings=settings;r->rt_shadows.valid=false;r->restir.history_valid=false;
  apply_quality(*r);
  return true;
}
bool initialize_lighting(WorldRenderer& r) {
  if(const auto* quality=SDL_getenv("OCTARYN_CLIENT_LIGHTING_QUALITY")) {
    const std::string_view value(quality);
    if(value=="low")r.lighting_settings.quality=LightingQuality::Low;
    else if(value=="medium")r.lighting_settings.quality=LightingQuality::Medium;
    else if(value=="ultra")r.lighting_settings.quality=LightingQuality::Ultra;
    else if(value!="high")return false;
  }
  if(const auto* debug=SDL_getenv("OCTARYN_CLIENT_LIGHTING_DEBUG"))r.lighting_settings.debug_view=unsigned(std::clamp(std::atoi(debug),0,20));
  if(r.lighting_settings.quality==LightingQuality::Low)r.lighting_settings.shadow_resolution=512;
  apply_quality(r);
  if(!initialize_rt_shadows(r) || !world_ray_debug_initialize(r) || !initialize_shadow_fallback(r) || !world_ddgi_initialize(r) || !world_restir_initialize(r))return false;
  if(SDL_getenv("OCTARYN_CLIENT_LIGHTING_FIXTURE")) {
    WorldLocalLight lights[3];
    lights[0].position_range={0,167,-4,32};lights[0].color_intensity={1,.35f,.1f,100};
    lights[1].position_range={4,170,-4,32};lights[1].color_intensity={.1f,.35f,1,160};lights[1].axis_v_type[3]=1;
    lights[2].position_range={-4,170,-4,32};lights[2].color_intensity={.2f,1,.3f,20};lights[2].axis_v_type[3]=2;
    if(!open_world_renderer_set_lights(&r,lights,3))return false;
  }
  return r.lighting_profile.initialize(r.device,SDL_getenv("OCTARYN_CLIENT_LIGHTING_PROFILE_PATH"));
}
bool render_lighting(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  world_block_lights_update(r);
  LightingGraph graph;
  graph.add(RaySceneResource,ProbeResource,[&]{return world_ddgi_update(r,commands);});
  graph.add(SurfaceResource|RaySceneResource,LocalResource,[&]{return world_restir_update(r,commands);});
  graph.add(SurfaceResource|RaySceneResource,ShadowResource,[&] {
    const bool rt=r.ray_enabled && world_ray_available(r) && r.lighting_settings.quality>=LightingQuality::High;
    if(rt)return update_rt_shadows(r,commands);
    r.rt_shadows.valid=false;
    r.lighting_profile.begin_pass(commands,LightingPass::SunTrace);
    const bool ok=update_shadow_fallback(r,commands);
    r.lighting_profile.mark(commands,LightingPass::SunTrace);return ok;
  });
  graph.add(SurfaceResource|ProbeResource|LocalResource|ShadowResource,SceneResource,[&] {
    r.lighting_profile.begin_pass(commands,LightingPass::Composition);
    const bool ok=composite_world_hdr(r,commands);
    r.lighting_profile.mark(commands,LightingPass::Composition);return ok;
  });
  return graph.execute(SurfaceResource|RaySceneResource) && world_ray_debug(r,commands);
}
}
