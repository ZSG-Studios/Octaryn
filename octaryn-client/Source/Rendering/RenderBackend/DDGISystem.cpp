#include "WorldRendererInternal.h"
#include "DDGISystem.h"
#include "DDGIOccupancy.h"
#include "DDGIVolumeConfig.h"
#include "DDGILightingChanges.h"
#include "SceneChanges.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace octaryn::client::rendering {
namespace {
float setting(const char* name,float fallback,float minimum,float maximum) {
  const char* value=SDL_getenv(name);if(!value)return fallback;
  char* end=nullptr;const float parsed=std::strtof(value,&end);
  return end && *end==0 && std::isfinite(parsed)?std::clamp(parsed,minimum,maximum):fallback;
}
bool allocate(WorldRenderer& r,Slang::ComPtr<rhi::IBuffer>& buffer,unsigned count,unsigned stride,const char* label) {
  rhi::BufferDesc desc{};desc.size=std::uint64_t(count)*stride;desc.elementSize=stride;desc.label=label;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  std::vector<std::uint8_t> zero(static_cast<std::size_t>(desc.size));
  return world_rhi_ok(r.device->createBuffer(desc,zero.data(),buffer.writeRef()));
}
bool bind(rhi::IShaderObject* root,const char* name,rhi::IBuffer* buffer) {
  auto cursor=rhi::ShaderCursor(root)[name];
  return !cursor.isValid() || world_rhi_ok(cursor.setBinding(rhi::Binding(buffer)));
}
template<class Value> bool uniform(rhi::IShaderObject* root,const char* name,const Value& value) {
  auto cursor=rhi::ShaderCursor(root)[name];
  return !cursor.isValid() || world_rhi_ok(cursor.setData(&value,sizeof(value)));
}
bool bind_volume(WorldRenderer&,rhi::IShaderObject*,DDGISystem&,bool,bool);
// The trace shader samples one uniformly random light from a prefix of the
// distance-sorted light list and scales by the prefix length; only lights that
// can reach this volume may occupy that prefix or per-light convergence
// scales with every distant fog-reach source instead of the near field.
unsigned volume_light_prefix(WorldRenderer& r,const DDGISystem& s) {
  const auto& lights=r.local_lighting.lights;
  if(lights.empty())return 0;
  double half2=0;
  for(unsigned axis=0;axis<3;++axis) {
    const double half=.5*double(s.config.counts[axis])*s.config.spacing;
    half2+=half*half;
  }
  const float reach=float(std::sqrt(half2));
  unsigned count=0;
  for(const auto& light:lights) {
    const float span=reach+light.position_range[3]+8.f;
    float squared=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=light.position_range[axis]-r.draw_uniforms[axis];
      squared+=delta*delta;
    }
    if(squared<=span*span)++count;
  }
  return count;
}
bool dispatch(WorldRenderer& r,DDGISystem& s,rhi::ICommandEncoder* commands,bool trace) {
  if(s.selected.empty())return true;
  if(!s.timing.begin(commands,r.active_frame,trace))return false;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(trace?s.trace.get():s.update.get());
  const unsigned count=static_cast<unsigned>(s.selected.size());
  auto* other=s.cell_centered?&r.ddgi:r.ddgi.fine_volume.get();
  bool ok=root && bind_volume(r,root,s,false,true) &&
    bind_volume(r,root,other?*other:s,true,trace&&other!=nullptr) &&
    bind(root,"ddgiSelection",s.selections[r.active_frame]) &&
     bind(root,"ddgiRays",s.rays) && uniform(root,"ddgiUpdateCount",count);
  if(ok && trace) {
    const std::array<float,4> sun{-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
    const std::array<float,4> sky{r.lighting.visual_sky_visibility,r.lighting.ambient_strength,
      r.sky.twilight_celestial_time[0],r.sky.twilight_celestial_time[1]};
    const unsigned lights=volume_light_prefix(r,s);
    const std::array<float,4> ignore=s.ignore_active?
      std::array<float,4>{float(s.ignore_voxel[0]),float(s.ignore_voxel[1]),float(s.ignore_voxel[2]),1}:
      std::array<float,4>{1e30f,1e30f,1e30f,0};
    ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && uniform(root,"ddgiSun",sun) && uniform(root,"ddgiSky",sky) &&
      bind(root,"localLights",r.local_lighting.light_buffer) && uniform(root,"ddgiLightCount",lights) &&
      uniform(root,"ddgiIgnoredBlock",ignore);
  }
  if(ok)pass->dispatchCompute(trace?(count*s.config.rays+63)/64:count,1,1);
  pass->end();
  if(ok)s.timing.end(commands,r.active_frame,trace,s.stats.scheduled_rays+64*s.stats.updated_probes,s.stats.updated_probes,s.selection_target,s.frame);
  return ok;
}
bool dispatch_seed(WorldRenderer& r,DDGISystem& s,rhi::ICommandEncoder* commands) {
  if(!s.seed)return true;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(s.seed.get());
  const unsigned count=s.stats.probe_count?s.stats.probe_count:1;
  bool ok=root && bind_volume(r,root,s,false,true) &&
    bind(root,"ddgiProbes",s.probes) && bind(root,"ddgiIrradiance",s.irradiance) &&
    bind(root,"ddgiDistance",s.distance) && bind(root,"ddgiVariability",s.variability) &&
    uniform(root,"ddgiProbeCount",count);
  if(ok)pass->dispatchCompute((count+63)/64,1,1);
  pass->end();return ok;
}
bool initialize_volume(WorldRenderer& r,DDGISystem& s) {
  auto& c=s.config;
  const unsigned count=s.available?c.counts[0]*c.counts[1]*c.counts[2]:1;
  if(!s.available) {c.counts={1,1,1};c.budget=c.rays=c.irradiance_resolution=c.visibility_resolution=1;}
  c.budget=std::min(c.budget,count);
  s.dispatch_capacity=std::min(count,std::max(c.budget,4096u));
  if(s.available && !s.timing.initialize(r.device,SDL_getenv("OCTARYN_DDGI_TIMING_PROFILE_PATH"),s.cell_centered))return false;
  if(!allocate(r,s.controls,count,sizeof(DDGIControl),"ddgi_grid_controls") ||
     !allocate(r,s.probes,count,sizeof(DDGIProbe),"ddgi_probe_state") ||
     !allocate(r,s.irradiance,count*c.irradiance_resolution*c.irradiance_resolution,16,"ddgi_irradiance") ||
     !allocate(r,s.distance,count*c.visibility_resolution*c.visibility_resolution,8,"ddgi_distance_moments") ||
     !allocate(r,s.variability,count,4,"ddgi_variability") ||
     !allocate(r,s.rays,s.dispatch_capacity*c.rays,32,"ddgi_ray_results"))return false;
  for(auto& selection:s.selections)if(!allocate(r,selection,s.dispatch_capacity,4,"ddgi_probe_selection"))return false;
  s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count,true);
  s.stats.probe_count=s.available?count:0;
  for(auto* resource:{s.controls.get(),s.probes.get(),s.irradiance.get(),s.distance.get(),s.rays.get(),s.variability.get(),
      s.selections[0].get(),s.selections[1].get()})s.stats.bytes+=resource->getDesc().size;
  if(s.available && (!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGITrace.slang","main",s.trace) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGIUpdate.slang","main",s.update) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGISeed.slang","main",s.seed)))return false;
  std::printf("world_ddgi enabled=%u probes=%u rays_per_probe=%u update_budget=%u spacing=%.2f bytes=%llu voxel=%u trace_distance=%.2f nominal_horizontal_radius=%.2f gpu_budget_ms_per_60=%.3f\n",
    unsigned(s.available),s.stats.probe_count,c.rays,c.budget,c.spacing,static_cast<unsigned long long>(s.stats.bytes),unsigned(s.cell_centered),
    c.max_distance,s.available?c.counts[0]*c.spacing*.5f:0.f,c.gpu_budget_milliseconds);
  return true;
}
bool bind_volume(WorldRenderer& r,rhi::IShaderObject* root,DDGISystem& s,bool fine,bool enabled) {
  if(!root)return false;
  const auto& c=s.config;
  const std::array<unsigned,4> grid{c.counts[0],c.counts[1],c.counts[2],enabled&&s.available&&r.ray_enabled?1u:0u};
  const std::array<int,4> origin{s.origin[0],s.origin[1],s.origin[2],s.cell_centered?1:0};
  const std::array<float,4> fadeOrigin{s.fade_origin[0],s.fade_origin[1],s.fade_origin[2],.2f};
  const std::array<float,4> parameters{c.spacing,c.hysteresis,c.max_distance,.2f};
  const std::array<unsigned,4> frame{static_cast<unsigned>(s.frame),c.rays,c.irradiance_resolution,c.visibility_resolution};
  const std::string prefix=fine?"ddgiFine":"ddgi";
  return bind(root,(prefix+"Controls").c_str(),s.controls) && bind(root,(prefix+"Probes").c_str(),s.probes) &&
    bind(root,(prefix+"Irradiance").c_str(),s.irradiance) && bind(root,(prefix+"Distance").c_str(),s.distance) &&
    bind(root,(prefix+"Variability").c_str(),s.variability) &&
    uniform(root,(prefix+"Grid").c_str(),grid) && uniform(root,(prefix+"Origin").c_str(),origin) &&
    uniform(root,(prefix+"FadeOrigin").c_str(),fadeOrigin) && uniform(root,(prefix+"Parameters").c_str(),parameters) &&
    uniform(root,(prefix+"Frame").c_str(),frame);
}
bool prepare_volume(WorldRenderer& r,DDGISystem& s,rhi::ICommandEncoder* commands) {
  double milliseconds=0;unsigned work=0;
  if(!s.timing.resolve(r.active_frame,milliseconds,work))return false;
  const double previous_budget=s.adaptive_budget;
  ddgi_budget_sample(s,milliseconds,work,s.timing.resolved_probes,s.timing.resolved_target);
  if(work && s.timing.profile) {
    s.timing.profile<<s.timing.resolved_frame<<','<<s.frame+1<<','<<r.active_frame<<','<<s.timing.resolved_probes<<','<<work<<','
      <<s.timing.trace_ms<<','<<s.timing.update_ms<<','<<s.time_seconds<<','<<s.frame_seconds<<','<<previous_budget<<','
      <<s.adaptive_budget<<','<<s.milliseconds_per_work<<','<<s.stats.pending_probes<<','<<s.stats.oldest_update_seconds<<','<<s.budget_credit<<','
      <<s.timing.resolved_target<<','<<unsigned(s.timing.resolved_probes<s.timing.resolved_target)<<','<<s.gpu_debt_seconds<<'\n';
  }
  ++s.frame;
  const auto now=std::chrono::steady_clock::now();
  if(s.update_clock.time_since_epoch().count()!=0) {
    s.frame_seconds=std::chrono::duration<double>(now-s.update_clock).count();
    s.time_seconds+=s.frame_seconds;
  }
  s.update_clock=now;
  ddgi_follow_opening(r,s);
  if(!r.scene_changes.for_each_since(s.scene_revision,[&](const SceneChange& change) {
    const bool opening=ddgi_ignore_covers(s,change.x,change.z);
    if(change.kind==SceneChangeKind::Added)return;
    if(change.kind==SceneChangeKind::Modified && !opening)return;
    if(change.kind==SceneChangeKind::AccelerationReady) {
      if(opening)ddgi_clear_ignore(s);
      // Sprite-only churn (torches) and topology-only halo remeshes rebuild the
      // BLAS without changing occluders; occupancy flips and light wakes own it.
      if(change.minor_build)return;
    } else if(change.kind==SceneChangeKind::Modified) {
      // Targeted edit: occupancy-flip rings plus light wakes cover it exactly.
      // The old whole-column box retraced the volume per torch click.
      return;
    }
    const float x=float(change.x)*32,z=float(change.z)*32;
    ddgi_invalidate(s,{x,float(change.min_y),z},{x+32,float(change.min_y+change.height),z+32},
      s.config.max_distance);
  }))ddgi_invalidate(s,{-1e30f,-1e30f,-1e30f},{1e30f,1e30f,1e30f});
  s.scene_revision=r.scene_changes.revision();
  const std::array<float,3> camera{r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2]};
  ddgi_scroll(s,camera);
  ddgi_classify_occupancy(r,s);
  ddgi_schedule(s,camera);
  if(s.controls_dirty) {
    if(!world_rhi_ok(commands->uploadBufferData(s.controls,0,s.control_data.size()*sizeof(DDGIControl),s.control_data.data())))return false;
    s.controls_dirty=false;
    // Cell versions, solid flags and hole markers only change here, which are
    // the only inputs the seed pass reads. Otherwise it would rescan 32k
    // probes a frame to do nothing.
    s.seed_needed=true;
  }
  if(!s.selected.empty() &&
      !world_rhi_ok(commands->uploadBufferData(s.selections[r.active_frame],0,s.selected.size()*sizeof(unsigned),s.selected.data())))return false;
  return true;
}
}
bool world_ddgi_initialize(WorldRenderer& r) {
  auto& s=r.ddgi;auto& c=s.config;
  const char* mode=SDL_getenv("OCTARYN_CLIENT_DDGI");
  s.available=world_ray_available(r) && (!mode || std::strcmp(mode,"off"));
  c.counts={unsigned(setting("OCTARYN_DDGI_COUNT_X",32,2,64)),unsigned(setting("OCTARYN_DDGI_COUNT_Y",12,2,32)),
    unsigned(setting("OCTARYN_DDGI_COUNT_Z",32,2,64))};
  c.spacing=setting("OCTARYN_DDGI_SPACING",8,.5f,32);
  c.hysteresis=setting("OCTARYN_DDGI_HYSTERESIS",.94f,0,.99f);
  c.max_distance=setting("OCTARYN_DDGI_MAX_DISTANCE",96,c.spacing,512);
  // Tier-0 burst budget: 64 fixed geometry + 112 rotating lighting rays.
  c.rays=unsigned(setting("OCTARYN_DDGI_RAYS",176,64,512));
  c.budget=unsigned(setting("OCTARYN_DDGI_BUDGET",96,1,512));
  c.irradiance_resolution=unsigned(setting("OCTARYN_DDGI_IRRADIANCE_RESOLUTION",6,2,16));
  c.visibility_resolution=unsigned(setting("OCTARYN_DDGI_VISIBILITY_RESOLUTION",8,2,16));
  // Startup env counts map onto the same block radii the runtime options use.
  auto& settings=r.lighting_settings;
  s.env_spacing=c.spacing;s.base_config=c;
  settings.ddgi_coarse_radius=s.available?unsigned(float(c.counts[0])*c.spacing/2):0;
  settings.ddgi_voxel_radius=s.available?6u:0;
  return world_ddgi_reconfigure(r);
}
bool world_ddgi_reconfigure(WorldRenderer& r) {
  auto& s=r.ddgi;
  const unsigned voxel=r.lighting_settings.ddgi_voxel_radius,coarse=r.lighting_settings.ddgi_coarse_radius;
  const char* mode=SDL_getenv("OCTARYN_CLIENT_DDGI");
  const bool enabled=world_ray_available(r) && (!mode || std::strcmp(mode,"off"));
  const auto reset=[](DDGISystem& s) {
    s.stats=DDGIStats{};s.control_data.clear();s.last_updates.clear();
     s.last_update_times.clear();s.time_seconds=0;s.update_clock={};
     s.response_updates.clear();s.observations.clear();s.wake_marked.clear();
    s.frame_seconds=1./60;s.budget_credit=0;
    s.timing={};s.milliseconds_per_work=s.adaptive_budget=s.gpu_debt_seconds=0;s.scheduled_work=0;s.selection_target=0;
    s.dirty.clear();s.selected.clear();s.occupancy.clear();
    s.schedule_scores.clear();s.schedule_order.clear();s.schedule_aged.clear();s.schedule_fresh.clear();
    s.schedule_chosen_stamps.clear();s.schedule_stamp=0;
    s.initialized=false;s.controls_dirty=true;s.seed_needed=true;s.frame=0;
    s.scene_revision=0;
    s.debug_boxes={};s.debug_box_cursor=0;
    s.dispatch_capacity=0;s.burst_frames=8;
    s.classified_origin={std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::max(),
      std::numeric_limits<std::int32_t>::max()};
    s.classified_ignore_voxel=s.classified_origin;
    s.classified_revision=~0ull;s.classified_ignore=false;
    s.sky_tops.clear();s.sky_tops_revision=~0ull;
    ddgi_clear_ignore(s);
  };
  const auto configure=[&](DDGISystem& volume,const DDGIConfig& config,bool available,bool fine) {
    // A coarse range adjustment must not reallocate or discard fine observations.
    if(volume.controls && volume.available==available && (!available || ddgi_same_config(volume.config,config)))return true;
    volume.config=config;volume.available=available;volume.cell_centered=fine;
    reset(volume);
    if(!initialize_volume(r,volume))return false;
    volume.scene_revision=r.scene_changes.revision();
    return true;
  };
  if(!configure(s,ddgi_volume_config(s.base_config,coarse,false,s.env_spacing),enabled && coarse>0,false))return false;
  if(enabled && voxel>0) {
    if(!s.fine_volume)s.fine_volume=std::make_unique<DDGISystem>();
    if(!configure(*s.fine_volume,ddgi_volume_config(s.base_config,voxel,true,1),true,true))return false;
  } else s.fine_volume.reset();
  return true;
}
bool world_ddgi_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.ddgi;
  return bind_volume(r,root,s,false,true) &&
    bind_volume(r,root,s.fine_volume?*s.fine_volume:s,true,bool(s.fine_volume));
}
bool world_ddgi_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.ddgi;
  for(auto* volume:{&s,s.fine_volume.get()})if(volume)
    volume->stats.updated_probes=volume->stats.scheduled_rays=volume->stats.invalidated_probes=0;
  if(!r.ray_enabled || (!s.available && !s.fine_volume))return true;
  world_ddgi_lighting_changes(r);
  if((s.available && !prepare_volume(r,s,commands)) ||
      (s.fine_volume && !prepare_volume(r,*s.fine_volume,commands)))return false;
  commands->globalBarrier();
  for(auto* volume:{&s,s.fine_volume.get()})if(volume && volume->available && volume->seed_needed) {
    if(!dispatch_seed(r,*volume,commands))return false;
    volume->seed_needed=false;
  }
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGITrace);
  // Both volumes read the previous hierarchy before either irradiance update.
  for(auto* volume:{&s,s.fine_volume.get()})if(volume && volume->available && !dispatch(r,*volume,commands,true))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGITrace);
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGIUpdate);
  for(auto* volume:{&s,s.fine_volume.get()})if(volume && volume->available && !dispatch(r,*volume,commands,false))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGIUpdate);
  commands->globalBarrier();
  if(r.frames%120==0) {
    const DDGIStats fine=s.fine_volume?s.fine_volume->stats:DDGIStats{};
    std::printf("world_ddgi frame=%llu scheduled_probes=%u ray_budget=%u invalidated=%u bytes=%llu probes=%u fine_probes=%u pending=%u fine_pending=%u oldest_seconds=%.3f fine_oldest_seconds=%.3f budget_per_60=%.1f fine_budget_per_60=%.1f\n",
      static_cast<unsigned long long>(s.frame),s.stats.updated_probes+fine.updated_probes,s.stats.scheduled_rays+fine.scheduled_rays,
      s.stats.invalidated_probes+fine.invalidated_probes,static_cast<unsigned long long>(s.stats.bytes+fine.bytes),s.stats.probe_count,fine.probe_count,
      s.stats.pending_probes,fine.pending_probes,s.stats.oldest_update_seconds,fine.oldest_update_seconds,
      s.adaptive_budget,s.fine_volume?s.fine_volume->adaptive_budget:0.);
  }
  return true;
}
}
