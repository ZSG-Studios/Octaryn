#include "WorldRendererInternal.h"
#include <fstream>
#include <filesystem>
#include <cmath>
namespace octaryn::client::rendering {
// Explicit capture only, after its frame fence; never read back in ordinary frames.
bool capture_lighting(WorldRenderer& r,const char* path) {
  std::uint32_t local[4]{};
  if(r.restir.counters && !world_rhi_ok(r.device->readBuffer(r.restir.counters,0,sizeof(local),local)))return false;
  std::uint32_t active{},sleeping{},inactive{},valid{};
  if(r.ddgi.available) {
    std::vector<DDGIProbe> probes(r.ddgi.stats.probe_count);
    if(!world_rhi_ok(r.device->readBuffer(r.ddgi.probes,0,probes.size()*sizeof(DDGIProbe),probes.data())))return false;
    for(const auto& p:probes) {
      for(const auto value:p.offset)if(!std::isfinite(value))return false;
      if(p.metadata[3])++valid;
      if(p.offset[3]==0)++active;else if(p.offset[3]==1)++inactive;else if(p.offset[3]==2)++sleeping;
    }
  }
  auto file=std::filesystem::path(path);file+=".lighting.json";
  std::ofstream out(file);
  out<<"{\n  \"local_visibility_rays\":"<<local[0]<<",\n  \"shaded_reservoirs\":"<<local[1]
    <<",\n  \"temporal_accepted\":"<<local[2]<<",\n  \"spatial_accepted\":"<<local[3]
    <<",\n  \"valid_probes\":"<<valid<<",\n  \"active_probes\":"<<active<<",\n  \"sleeping_probes\":"<<sleeping
    <<",\n  \"inactive_probes\":"<<inactive<<",\n  \"scene_revision\":"<<r.scene_changes.revision()
    <<",\n  \"block_source_count\":"<<r.block_lights.source_count<<",\n  \"block_selected_count\":"<<r.block_lights.selected_count
    <<",\n  \"local_light_count\":"<<r.restir.lights.size()
    <<",\n  \"local_shadow_valid\":"<<(r.local_shadows.valid?1:0)<<",\n  \"local_shadow_selected\":"<<r.local_shadows.selected
    <<",\n  \"local_shadow_draws\":"<<r.local_shadows.draws<<",\n  \"local_shadow_updates\":"<<r.local_shadows.map_updates<<"\n}\n";
  return bool(out);
}
}
