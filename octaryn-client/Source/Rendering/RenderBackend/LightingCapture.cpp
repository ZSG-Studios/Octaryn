#include "WorldRendererInternal.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
struct ProbeEnergy {std::uint32_t valid{},texels{};double sum{},maximum{};};
void write_energy(std::ostream& out,const char* name,const ProbeEnergy& energy) {
  out<<",\n  \""<<name<<"_valid_probes\":"<<energy.valid
    <<",\n  \""<<name<<"_irradiance_texels\":"<<energy.texels
    <<",\n  \""<<name<<"_irradiance_sum\":"<<energy.sum
    <<",\n  \""<<name<<"_irradiance_mean\":"<<(energy.texels?energy.sum/energy.texels:0)
    <<",\n  \""<<name<<"_irradiance_max\":"<<energy.maximum;
}
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
  std::array<ProbeEnergy,2> energies{};
  for(const auto* volume:{&r.ddgi,r.ddgi.fine_volume.get()}) {
    if(volume==r.ddgi.fine_volume.get() && states.is_open())states<<"],\"fine_probes\":[";
    if(!volume || !volume->available)continue;
    std::vector<DDGIProbe> probes(volume->stats.probe_count);
    if(!world_rhi_ok(r.device->readBuffer(volume->probes,0,probes.size()*sizeof(DDGIProbe),probes.data())))return false;
    const unsigned resolution=volume->config.irradiance_resolution,texels=resolution*resolution;
    std::vector<std::array<float,4>> irradiance(probes.size()*texels);
    if(!world_rhi_ok(r.device->readBuffer(volume->irradiance,0,irradiance.size()*sizeof(irradiance[0]),irradiance.data())))return false;
    auto& energy=energies[volume==&r.ddgi?0:1];
    for(unsigned index=0;index<probes.size();++index) {
      const auto& p=probes[index];
      if(states.is_open())write_probe(states,index,volume->control_data[index],p);
      for(const auto value:p.offset)if(!std::isfinite(value))return false;
      const auto& control=volume->control_data[index];
      const bool validProbe=p.metadata[3] && p.metadata[1] && p.metadata[0]==control.version &&
        p.offset[3]!=1 && !control.padding[0] && !control.padding[1] &&
        !(control.padding[2]==2 && control.refresh_frame>p.metadata[1]);
      if(validProbe) {
        ++valid;++energy.valid;
        for(unsigned texel=0;texel<texels;++texel) {
          const auto& value=irradiance[index*texels+texel];
          for(float channel:value)if(!std::isfinite(channel))return false;
          const double luminance=.2126*value[0]+.7152*value[1]+.0722*value[2];
          energy.sum+=luminance;energy.maximum=std::max(energy.maximum,luminance);++energy.texels;
        }
      }
      if(p.offset[3]==0)++active;else if(p.offset[3]==1)++inactive;else if(p.offset[3]==2)++sleeping;
    }
  }
  if(states.is_open()) {states<<"]}\n";if(!states)return false;}
  auto file=std::filesystem::path(path);file+=".lighting.json";
  std::ofstream out(file);
  const auto fine=r.ddgi.fine_volume?r.ddgi.fine_volume->stats:DDGIStats{};
  out<<"{\n  \"local_visibility_rays\":"<<local[0]<<",\n  \"shaded_pixels\":"<<local[1]
    <<",\n  \"evaluated_lights\":"<<local[2]<<",\n  \"tile_overflow_pixels\":"<<local[3]
    <<",\n  \"valid_probes\":"<<valid<<",\n  \"active_probes\":"<<active<<",\n  \"sleeping_probes\":"<<sleeping
    <<",\n  \"inactive_probes\":"<<inactive<<",\n  \"scene_revision\":"<<r.scene_changes.revision()
    <<",\n  \"ddgi_total_bytes\":"<<r.ddgi.stats.bytes+fine.bytes
    <<",\n  \"ddgi_total_ray_budget\":"<<r.ddgi.stats.scheduled_rays+fine.scheduled_rays
    <<",\n  \"ddgi_coarse_enabled\":"<<(r.ddgi.available && r.ray_enabled?"true":"false")
    <<",\n  \"ddgi_fine_enabled\":"<<(r.ddgi.fine_volume && r.ray_enabled?"true":"false")
    <<",\n  \"ddgi_coarse_probes\":"<<r.ddgi.stats.probe_count<<",\n  \"ddgi_fine_probes\":"<<fine.probe_count
    <<",\n  \"ddgi_coarse_spacing\":"<<r.ddgi.config.spacing
    <<",\n  \"ddgi_coarse_counts\":["<<r.ddgi.config.counts[0]<<','<<r.ddgi.config.counts[1]<<','<<r.ddgi.config.counts[2]<<']'
    <<",\n  \"ddgi_coarse_coverage_min\":["<<r.ddgi.fade_origin[0]*r.ddgi.config.spacing<<','
      <<r.ddgi.fade_origin[1]*r.ddgi.config.spacing<<','<<r.ddgi.fade_origin[2]*r.ddgi.config.spacing<<']'
    <<",\n  \"ddgi_coarse_coverage_max\":["<<(r.ddgi.fade_origin[0]+float(r.ddgi.config.counts[0])-2)*r.ddgi.config.spacing<<','
      <<(r.ddgi.fade_origin[1]+float(r.ddgi.config.counts[1])-2)*r.ddgi.config.spacing<<','
      <<(r.ddgi.fade_origin[2]+float(r.ddgi.config.counts[2])-2)*r.ddgi.config.spacing<<']'
    <<",\n  \"ddgi_requested_coarse_radius\":"<<r.lighting_settings.ddgi_coarse_radius
    <<",\n  \"ddgi_requested_voxel_radius\":"<<r.lighting_settings.ddgi_voxel_radius
    <<",\n  \"ddgi_pending_probes\":"<<r.ddgi.stats.pending_probes+fine.pending_probes
    <<",\n  \"ddgi_oldest_update_seconds\":"<<std::max(r.ddgi.stats.oldest_update_seconds,fine.oldest_update_seconds)
    <<",\n  \"block_source_count\":"<<r.block_lights.source_count<<",\n  \"block_selected_count\":"<<r.block_lights.selected_count
    <<",\n  \"local_light_count\":"<<r.local_lighting.lights.size()
    <<",\n  \"local_shadow_valid\":"<<(r.local_shadows.valid?1:0)<<",\n  \"local_shadow_selected\":"<<r.local_shadows.selected
    <<",\n  \"local_shadow_draws\":"<<r.local_shadows.draws<<",\n  \"local_shadow_updates\":"<<r.local_shadows.map_updates;
  write_energy(out,"coarse",energies[0]);write_energy(out,"fine",energies[1]);out<<"\n}\n";
  return bool(out);
}
}
