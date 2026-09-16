#include "WorldRendererInternal.h"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
void write_probe(std::ostream& out,unsigned index,const DDGIControl& control,const DDGIProbe& probe) {
  if(index)out<<',';
  out<<"{\"index\":"<<index<<",\"cell\":["<<control.cell[0]<<','<<control.cell[1]<<','<<control.cell[2]
    <<"],\"version\":"<<control.version<<",\"refresh\":"<<control.refresh_frame<<",\"offset\":["
    <<probe.offset[0]<<','<<probe.offset[1]<<','<<probe.offset[2]<<','<<probe.offset[3]
    <<"],\"metadata\":["<<probe.metadata[0]<<','<<probe.metadata[1]<<','<<probe.metadata[2]<<','<<probe.metadata[3]<<"]}";
}
}
// Explicit capture only, after its frame fence; never read back in ordinary frames.
bool capture_lighting(WorldRenderer& r,const char* path) {
  std::ofstream states;
  const auto* captureStates=SDL_getenv("OCTARYN_CLIENT_CAPTURE_DDGI_STATES");
  if(captureStates && std::strcmp(captureStates,"1")==0) {
    auto statePath=std::filesystem::path(path);statePath+=".ddgi-probes.json";states.open(statePath);
    if(!states)return false;
    states<<"{\"frame\":"<<r.ddgi.frame<<",\"probes\":[";
  }
  std::uint32_t local[4]{};
  if(r.local_lighting.counters && !world_rhi_ok(r.device->readBuffer(r.local_lighting.counters,0,sizeof(local),local)))return false;
  std::uint32_t active{},sleeping{},inactive{},valid{};
  if(r.ddgi.available) {
    std::vector<DDGIProbe> probes(r.ddgi.stats.probe_count);
    if(!world_rhi_ok(r.device->readBuffer(r.ddgi.probes,0,probes.size()*sizeof(DDGIProbe),probes.data())))return false;
    for(unsigned index=0;index<probes.size();++index) {
      const auto& p=probes[index];
      if(states.is_open())write_probe(states,index,r.ddgi.control_data[index],p);
      for(const auto value:p.offset)if(!std::isfinite(value))return false;
      if(p.metadata[3])++valid;
      if(p.offset[3]==0)++active;else if(p.offset[3]==1)++inactive;else if(p.offset[3]==2)++sleeping;
    }
  }
  if(states.is_open()) {states<<"]}\n";if(!states)return false;}
  auto file=std::filesystem::path(path);file+=".lighting.json";
  std::ofstream out(file);
  out<<"{\n  \"local_visibility_rays\":"<<local[0]<<",\n  \"shaded_pixels\":"<<local[1]
    <<",\n  \"evaluated_lights\":"<<local[2]<<",\n  \"tile_overflow_pixels\":"<<local[3]
    <<",\n  \"valid_probes\":"<<valid<<",\n  \"active_probes\":"<<active<<",\n  \"sleeping_probes\":"<<sleeping
    <<",\n  \"inactive_probes\":"<<inactive<<",\n  \"scene_revision\":"<<r.scene_changes.revision()
    <<",\n  \"ddgi_total_bytes\":"<<r.ddgi.stats.bytes
    <<",\n  \"ddgi_total_ray_budget\":"<<r.ddgi.stats.scheduled_rays
    <<",\n  \"block_source_count\":"<<r.block_lights.source_count<<",\n  \"block_selected_count\":"<<r.block_lights.selected_count
    <<",\n  \"local_light_count\":"<<r.local_lighting.lights.size()
    <<",\n  \"local_shadow_valid\":"<<(r.local_shadows.valid?1:0)<<",\n  \"local_shadow_selected\":"<<r.local_shadows.selected
    <<",\n  \"local_shadow_draws\":"<<r.local_shadows.draws<<",\n  \"local_shadow_updates\":"<<r.local_shadows.map_updates<<"\n}\n";
  return bool(out);
}
}
