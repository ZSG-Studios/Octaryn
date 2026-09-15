#include "WorldRendererInternal.h"
#include "LightingSystem.h"
#include "LightingGraph.h"
#include "DDGIDebug.h"
#include <cstdlib>
#include <string_view>
namespace octaryn::client::rendering {
namespace {
void apply_quality(WorldRenderer& r) {
  r.local_lighting.settings.tile_capacity=64;
  const auto view=r.lighting_settings.debug_view;
  r.local_lighting.settings.debug=view>=13 && view<=20?view-12:0;
}
}
bool open_world_renderer_set_lighting_options(WorldRenderer* r,const LightingSettings& settings) {
  if(!r || settings.quality>LightingQuality::Ultra || !std::isfinite(settings.sun_angular_radius) ||
     !std::isfinite(settings.shadow_history_weight) || settings.sun_angular_radius<0 || settings.sun_angular_radius>.1f ||
     settings.shadow_history_weight<0 || settings.shadow_history_weight>=1 || settings.shadow_resolution<256 ||
     settings.shadow_resolution>2048 || settings.debug_view>27)return false;
  if(settings.shadow_resolution!=r->lighting_settings.shadow_resolution && !open_world_renderer_flush(r))return false;
  r->lighting_settings=settings;r->rt_shadows.valid=false;
  apply_quality(*r);
  return true;
}
void open_world_renderer_set_lighting_debug(WorldRenderer* r,unsigned debug_view) {
  if(!r)return;
  r->lighting_settings.debug_view=std::min(debug_view,27u);
  apply_quality(*r);
}
void open_world_renderer_set_trace_ranges(WorldRenderer* r,float shadow_distance,float reflection_distance) {
  if(!r)return;
  auto& settings=r->lighting_settings;
  settings.shadow_distance=std::isfinite(shadow_distance)?std::max(shadow_distance,0.f):0.f;
  settings.reflection_distance=std::isfinite(reflection_distance)?std::max(reflection_distance,0.f):0.f;
}
bool open_world_renderer_set_ddgi_range(WorldRenderer* r,unsigned voxel_radius,unsigned coarse_radius) {
  if(!r)return false;
  auto& settings=r->lighting_settings;
  if(settings.ddgi_voxel_radius==voxel_radius && settings.ddgi_coarse_radius==coarse_radius)return true;
  settings.ddgi_voxel_radius=std::min(voxel_radius,32u);
  settings.ddgi_coarse_radius=std::min(coarse_radius,1024u);
  return open_world_renderer_flush(r) && world_ddgi_reconfigure(*r);
}
bool initialize_lighting(WorldRenderer& r) {
  if(const auto* quality=SDL_getenv("OCTARYN_CLIENT_LIGHTING_QUALITY")) {
    const std::string_view value(quality);
    if(value=="low")r.lighting_settings.quality=LightingQuality::Low;
    else if(value=="medium")r.lighting_settings.quality=LightingQuality::Medium;
    else if(value=="ultra")r.lighting_settings.quality=LightingQuality::Ultra;
    else if(value!="high")return false;
  }
  if(const auto* debug=SDL_getenv("OCTARYN_CLIENT_LIGHTING_DEBUG"))r.lighting_settings.debug_view=unsigned(std::clamp(std::atoi(debug),0,27));
  if(r.lighting_settings.quality==LightingQuality::Low)r.lighting_settings.shadow_resolution=512;
  apply_quality(r);
  if(!initialize_rt_shadows(r) || !world_ray_debug_initialize(r) || !initialize_shadow_fallback(r) || !world_ddgi_initialize(r) ||
     !world_ddgi_debug_initialize(r) || !world_local_lighting_initialize(r))return false;
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
  // Publish one immutable light list for all direct and indirect consumers.
  if(!graph.add(0,LightResource,[&]{return world_local_lighting_prepare(r,commands);}) ||
     !graph.add(RaySceneResource|LightResource,ProbeResource,[&]{return world_ddgi_update(r,commands);}) ||
     !graph.add(SurfaceResource|RaySceneResource|LightResource,LocalResource,[&]{return world_local_lighting_update(r,commands);}))return false;
  if(!graph.add(SurfaceResource|RaySceneResource,ShadowResource,[&] {
    const bool rt=r.ray_enabled && world_ray_available(r) && r.lighting_settings.quality>=LightingQuality::High &&
      r.lighting_settings.shadow_distance>0;
    if(rt)return update_rt_shadows(r,commands);
    r.rt_shadows.valid=false;
    r.lighting_profile.begin_pass(commands,LightingPass::SunTrace);
    const bool ok=update_shadow_fallback(r,commands);
    r.lighting_profile.mark(commands,LightingPass::SunTrace);return ok;
  }))return false;
  if(!graph.add(SurfaceResource|ProbeResource|LocalResource|ShadowResource,SceneResource,[&] {
    r.lighting_profile.begin_pass(commands,LightingPass::Composition);
    const bool ok=composite_world_hdr(r,commands);
    r.lighting_profile.mark(commands,LightingPass::Composition);return ok;
  }))return false;
  return graph.execute(SurfaceResource|RaySceneResource) && world_ray_debug(r,commands) && world_ddgi_debug(r,commands);
}
}
