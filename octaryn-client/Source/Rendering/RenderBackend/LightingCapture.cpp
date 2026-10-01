#include "WorldRendererInternal.h"
#include <fstream>
#include <filesystem>
namespace octaryn::client::rendering {
// Explicit capture only, after its frame fence; never read back in ordinary frames.
bool capture_lighting(WorldRenderer& r,const char* path) {
  const auto ray=world_ray_stats(r);
  std::uint32_t local[4]{};
  if(r.local_lighting.counters && !world_rhi_ok(r.device->readBuffer(r.local_lighting.counters,0,sizeof(local),local)))return false;
  auto file=std::filesystem::path(path);file+=".lighting.json";
  std::ofstream out(file);
  out<<"{\n  \"render_frame\":"<<r.frames
    <<",\n  \"gi_mode\":\"direct\""
    <<",\n  \"ray_enabled\":"<<(r.ray_enabled?"true":"false")
    <<",\n  \"shadow_distance\":"<<r.lighting_settings.shadow_distance
    <<",\n  \"reflection_distance\":"<<r.lighting_settings.reflection_distance
    <<",\n  \"lighting_debug_view\":"<<r.lighting_settings.debug_view
    <<",\n  \"ray_coverage_complete\":"<<(world_ray_coverage_complete(r)?"true":"false")
    <<",\n  \"ray_resident_columns\":"<<ray.resident_columns<<",\n  \"ray_ready_columns\":"<<ray.ready_columns
    <<",\n  \"ray_pending_columns\":"<<ray.pending_columns<<",\n  \"ray_active_jobs\":"<<ray.active_jobs
    <<",\n  \"local_visibility_rays\":"<<local[0]<<",\n  \"shaded_pixels\":"<<local[1]
    <<",\n  \"evaluated_lights\":"<<local[2]<<",\n  \"tile_overflow_pixels\":"<<local[3]
    <<",\n  \"scene_revision\":"<<r.scene_changes.revision()
    <<",\n  \"local_light_count\":"<<r.local_lighting.lights.size()
    <<",\n  \"sky_light_direction\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.sky.light_direction_sky[i];
  out<<"],\n  \"sky_time\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.sky.twilight_celestial_time[i];
  out<<"]";
  out<<"\n}\n";
  return bool(out);
}
}
