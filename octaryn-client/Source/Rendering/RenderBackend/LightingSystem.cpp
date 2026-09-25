#include "WorldRendererInternal.h"
#include "LightingSystem.h"
#include "LightingGraph.h"
#include "ShadowQuality.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace octaryn::client::rendering {
namespace {
void apply_quality(WorldRenderer& r) {
  r.local_lighting.settings.tile_capacity=64;
  const auto view=r.lighting_settings.debug_view;
  r.local_lighting.settings.debug=view>=13 && view<=20?view-12:0;
}
}
bool open_world_renderer_set_lighting_options(WorldRenderer* r,const LightingSettings& settings) {
  if(!r || !std::isfinite(settings.sun_angular_radius) ||
     !std::isfinite(settings.shadow_history_weight) || settings.sun_angular_radius<0 || settings.sun_angular_radius>.1f ||
     settings.shadow_history_weight<0 || settings.shadow_history_weight>=1 || settings.shadow_resolution<256 ||
      settings.shadow_resolution>2048 || settings.debug_view>31 ||
      settings.reflection_quality>3 || settings.shadow_quality>3)return false;
  if(settings.shadow_resolution!=r->lighting_settings.shadow_resolution && !open_world_renderer_flush(r))return false;
  r->lighting_settings=settings;r->rt_shadows.valid=false;r->map_reflections.valid=false;
  apply_quality(*r);
  return true;
}
void open_world_renderer_set_lighting_debug(WorldRenderer* r,unsigned debug_view) {
  if(!r)return;
  r->lighting_settings.debug_view=std::min(debug_view,31u);
  apply_quality(*r);
}
void open_world_renderer_set_reflection_quality(WorldRenderer* r,unsigned quality) {
  if(!r || quality>3 || r->lighting_settings.reflection_quality==quality)return;
  r->lighting_settings.reflection_quality=quality;
  r->map_reflections.valid=false;
  std::printf("world_reflection_quality tier=%u history_reset=1\n",quality);
}
void open_world_renderer_set_shadow_quality(WorldRenderer* r,unsigned quality) {
  if(!r || quality>3 || r->lighting_settings.shadow_quality==quality)return;
  const auto policy=shadow_quality_policy(quality);
  if(r->lighting_settings.shadow_resolution!=policy.raster_resolution && !open_world_renderer_flush(r))return;
  r->lighting_settings.shadow_quality=quality;
  r->lighting_settings.shadow_resolution=policy.raster_resolution;
  r->rt_shadows.valid=false;
  std::printf("world_shadow_quality tier=%u raster_resolution=%u history_reset=1\n",quality,policy.raster_resolution);
}
void open_world_renderer_set_raster_shadows(WorldRenderer* r,int enabled) {
  if(r)r->lighting_settings.raster_shadows=enabled!=0;
}
void open_world_renderer_set_trace_ranges(WorldRenderer* r,float shadow_distance,float reflection_distance) {
  if(!r)return;
  auto& settings=r->lighting_settings;
  settings.shadow_distance=std::isfinite(shadow_distance)?std::max(shadow_distance,0.f):0.f;
  settings.reflection_distance=std::isfinite(reflection_distance)?std::max(reflection_distance,0.f):0.f;
}
bool initialize_lighting(WorldRenderer& r) {
  std::puts("world_gi mode=direct");
  if(const auto* debug=SDL_getenv("OCTARYN_CLIENT_LIGHTING_DEBUG"))r.lighting_settings.debug_view=unsigned(std::clamp(std::atoi(debug),0,31));
  apply_quality(r);
  if(!initialize_rt_shadows(r) || !world_ray_debug_initialize(r) || !world_local_lighting_initialize(r))return false;
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
  LightingGraph graph;
  // Publish one immutable light list for all direct and indirect consumers.
  const auto indirect_reads=SurfaceResource|RaySceneResource|LocalResource|ShadowResource;
  if(!graph.add(0,LightResource,[&]{return world_local_lighting_prepare(r,commands);}) ||
     !graph.add(SurfaceResource|RaySceneResource|LightResource,LocalResource,
       [&]{return world_local_lighting_update(r,commands);}))return false;
  if(!graph.add(SurfaceResource|RaySceneResource,ShadowResource,[&] {
    const bool rt=r.ray_effects && r.ray_enabled && world_ray_available(r) && world_ray_coverage_complete(r) &&
      r.lighting_settings.shadow_distance>0;
    if(rt)return update_rt_shadows(r,commands);
    r.rt_shadows.valid=false;
    // A zero traced range disables the RT sun pass, but it must not leave the
    // visibility target at its clear value of one. That turns the direct sun
    // term into an unoccluded light through every surface. The voxel raster
    // clipmap fallback is gone with the voxel world: without ray tracing the
    // mesh world renders unoccluded sun.
    r.target().hdr.ray_shadows=false;return true;
  }))return false;
  if(!graph.add(indirect_reads,SceneResource,[&] {
    r.lighting_profile.begin_pass(commands,LightingPass::Composition);
    const bool ok=composite_world_hdr(r,commands);
    r.lighting_profile.mark(commands,LightingPass::Composition);
    return ok;
  }))return false;
  return graph.execute(SurfaceResource|RaySceneResource) && world_ray_debug(r,commands);
}
}
