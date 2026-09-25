#include "WorldRendererInternal.h"
#include <fstream>
#include <filesystem>
namespace octaryn::client::rendering {
bool capture_block_transport_values(WorldRenderer&,const char*);
bool capture_dynamic_receiver_values(WorldRenderer&,std::ostream&);
// Explicit capture only, after its frame fence; never read back in ordinary frames.
bool capture_lighting(WorldRenderer& r,const char* path) {
  if(!capture_block_transport_values(r,path))return false;
  const auto ray=world_ray_stats(r);
  std::uint32_t local[4]{};
  if(r.local_lighting.counters && !world_rhi_ok(r.device->readBuffer(r.local_lighting.counters,0,sizeof(local),local)))return false;
  std::array<std::uint32_t,12> transport{};
  if(r.block_gi.counters && !world_rhi_ok(r.device->readBuffer(r.block_gi.counters,0,sizeof(transport),transport.data())))return false;
  auto file=std::filesystem::path(path);file+=".lighting.json";
  std::ofstream out(file);
  out<<"{\n  \"render_frame\":"<<r.frames
    <<",\n  \"gi_mode\":\""<<gi_mode_name(r.gi_mode)<<"\""
    <<",\n  \"block_transport_active\":"<<(r.block_gi.active?"true":"false")
    <<",\n  \"block_transport_frames_since_reset\":"<<r.block_gi.stats.frames_since_reset
    <<",\n  \"block_transport_coverage_valid\":"<<(r.block_gi.stats.coverage_valid?"true":"false")
    <<",\n  \"block_transport_ready\":"<<(r.block_gi.stats.transport_ready?"true":"false")
    <<",\n  \"block_transport_statistics_frame\":"<<r.block_gi.stats.measured_frame
    <<",\n  \"block_transport_admission_sweeps\":"<<r.block_gi.stats.admission_sweeps
    <<",\n  \"block_transport_admission_clean\":"<<(r.block_gi.stats.mandatory_ready?"true":"false")
    <<",\n  \"block_transport_measured_admission_sweeps\":"<<r.block_gi.stats.measured_sweeps
    <<",\n  \"block_transport_pinned_rows\":"<<r.block_gi.stats.pinned_rows
    <<",\n  \"block_transport_contributor_limit\":"<<r.block_gi.stats.contributor_limit
    <<",\n  \"block_transport_contributor_start_frame\":"<<r.block_gi.stats.contributor_start_frame
    <<",\n  \"block_transport_contributor_admission_failures\":"<<transport[1]-transport[11]
    <<",\n  \"block_transport_admission_faces\":"<<r.block_gi.stats.admission_faces
    <<",\n  \"block_transport_admission_columns\":"<<r.block_gi.stats.admission_columns
    <<",\n  \"block_transport_epoch\":"<<r.block_gi.epoch
    <<",\n  \"block_transport_radiance_epoch\":"<<r.block_gi.radiance_epoch
    <<",\n  \"block_transport_scheduled_rows\":"<<r.block_gi.stats.scheduled_rows
    <<",\n  \"block_transport_ready_rows\":"<<r.block_gi.stats.ready_rows
    <<",\n  \"block_transport_selection_occupied\":"<<r.block_gi.stats.selection_occupied
    <<",\n  \"block_transport_selection_wraps\":"<<r.block_gi.stats.selection_wraps
    <<",\n  \"block_transport_player_visibility_calls\":"<<r.block_gi.stats.player_visibility_calls
    <<",\n  \"block_transport_player_history_visibility_calls\":"<<r.block_gi.stats.player_history_visibility_calls
    <<",\n  \"block_transport_eviction_passes\":"<<r.block_gi.stats.eviction_passes
    <<",\n  \"block_transport_gpu_bytes\":"<<r.block_gi.stats.gpu_bytes
    <<",\n  \"block_transport_surface_key_bytes\":"<<r.block_gi.stats.surface_key_bytes
    <<",\n  \"block_transport_total_gpu_bytes\":"<<r.block_gi.stats.total_gpu_bytes
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
    <<",\n  \"gi_admission\":\""<<(r.gi_mode==GiMode::BlockTransport?"world_geometry":"disabled")<<"\""
    <<",\n  \"block_source_count\":"<<r.block_lights.source_count<<",\n  \"block_selected_count\":"<<r.block_lights.selected_count
    <<",\n  \"local_light_count\":"<<r.local_lighting.lights.size()
    <<",\n  \"local_shadow_valid\":"<<(r.local_shadows.valid?1:0)<<",\n  \"local_shadow_selected\":"<<r.local_shadows.selected
    <<",\n  \"local_shadow_draws\":"<<r.local_shadows.draws<<",\n  \"local_shadow_updates\":"<<r.local_shadows.map_updates
    <<",\n  \"block_transport_counters\":[";
  for(unsigned i=0;i<transport.size();++i)out<<(i?",":"")<<transport[i];
  out<<"],\n  \"sky_light_direction\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.sky.light_direction_sky[i];
  out<<"],\n  \"sky_time\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.sky.twilight_celestial_time[i];
  out<<"],\n  \"gi_sun\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.block_gi.sun[i];
  out<<"],\n  \"gi_sky\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<r.block_gi.sky[i];
  out<<"]";
  if(!capture_dynamic_receiver_values(r,out))return false;
  out<<"\n}\n";
  return bool(out);
}
}
