#include "WorldRendererInternal.h"
#include "DDGISystem.h"
#include "DDGIOccupancy.h"
#include "DDGIVolumeConfig.h"
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
bool dispatch(WorldRenderer& r,DDGISystem& s,rhi::ICommandEncoder* commands,bool trace) {
  if(s.selected.empty())return true;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(trace?s.trace.get():s.update.get());
  const unsigned count=static_cast<unsigned>(s.selected.size());
  auto* other=s.cell_centered?&r.ddgi:r.ddgi.fine_volume.get();
  bool ok=root && bind_volume(r,root,s,false,true) &&
    bind_volume(r,root,other?*other:s,true,trace&&other!=nullptr) &&
    bind(root,"ddgiSelection",s.selections[r.active_frame]) &&
     bind(root,"ddgiRays",s.rays) && uniform(root,"ddgiUpdateCount",count);
  if(ok && !trace)ok=bind(root,"ddgiHistoryIntervals",s.history_intervals[r.active_frame]);
  if(ok && trace) {
    const std::array<float,4> sun{-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
    const std::array<float,4> sky{r.lighting.visual_sky_visibility,r.lighting.ambient_strength,
      r.sky.twilight_celestial_time[0],r.sky.twilight_celestial_time[1]};
    const unsigned lights=static_cast<unsigned>(r.local_lighting.lights.size());
    const std::array<float,4> ignore=s.ignore_active?
      std::array<float,4>{float(s.ignore_voxel[0]),float(s.ignore_voxel[1]),float(s.ignore_voxel[2]),1}:
      std::array<float,4>{1e30f,1e30f,1e30f,0};
    ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && uniform(root,"ddgiSun",sun) && uniform(root,"ddgiSky",sky) &&
      bind(root,"localLights",r.local_lighting.light_buffer) && uniform(root,"ddgiLightCount",lights) &&
      uniform(root,"ddgiIgnoredBlock",ignore);
  }
  if(ok)pass->dispatchCompute(trace?(count*s.config.rays+63)/64:count,1,1);
  pass->end();return ok;
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
  s.dispatch_capacity=std::min(count,std::max(c.budget,1024u));
  if(!allocate(r,s.controls,count,sizeof(DDGIControl),"ddgi_grid_controls") ||
     !allocate(r,s.probes,count,sizeof(DDGIProbe),"ddgi_probe_state") ||
     !allocate(r,s.irradiance,count*c.irradiance_resolution*c.irradiance_resolution,16,"ddgi_irradiance") ||
     !allocate(r,s.distance,count*c.visibility_resolution*c.visibility_resolution,8,"ddgi_distance_moments") ||
     !allocate(r,s.variability,count,4,"ddgi_variability") ||
     !allocate(r,s.rays,s.dispatch_capacity*c.rays,32,"ddgi_ray_results"))return false;
  for(auto& selection:s.selections)if(!allocate(r,selection,s.dispatch_capacity,4,"ddgi_probe_selection"))return false;
  for(auto& interval:s.history_intervals)if(!allocate(r,interval,s.dispatch_capacity,4,"ddgi_history_intervals"))return false;
  s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count,true);
  s.stats.probe_count=s.available?count:0;
  for(auto* resource:{s.controls.get(),s.probes.get(),s.irradiance.get(),s.distance.get(),s.rays.get(),s.variability.get(),
      s.selections[0].get(),s.selections[1].get(),s.history_intervals[0].get(),s.history_intervals[1].get()})s.stats.bytes+=resource->getDesc().size;
  if(s.available && (!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGITrace.slang","main",s.trace) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGIUpdate.slang","main",s.update) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGISeed.slang","main",s.seed)))return false;
  std::printf("world_ddgi enabled=%u probes=%u rays_per_probe=%u update_budget=%u spacing=%.2f bytes=%llu voxel=%u\n",
    unsigned(s.available),s.stats.probe_count,c.rays,c.budget,c.spacing,static_cast<unsigned long long>(s.stats.bytes),unsigned(s.cell_centered));
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
  s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  ++s.frame;
  const auto now=std::chrono::steady_clock::now();
  if(s.update_clock.time_since_epoch().count()!=0) {
    s.frame_seconds=std::chrono::duration<double>(now-s.update_clock).count();
    s.time_seconds+=s.frame_seconds;
  }
  s.update_clock=now;
  // Placement/removal wakes immediately; intensity-only revisions (flicker) are
  // rate-limited so a 20 Hz fire cannot wake its region every frame.
  // Positional match: moves read as remove+add, flicker stays gentle. The fast
  // path exploits the stable publication order; full matching is the fallback.
  const auto& lights=r.local_lighting.lights;
  std::vector<char> old_matched(s.light_bounds.size(),0),new_matched(lights.size(),0);
  bool ordered=lights.size()==s.light_bounds.size();
  if(ordered)for(unsigned i=0;i<lights.size();++i) {
    const auto dx=lights[i].position_range[0]-s.light_bounds[i][0],dy=lights[i].position_range[1]-s.light_bounds[i][1],
      dz=lights[i].position_range[2]-s.light_bounds[i][2];
    if(dx*dx+dy*dy+dz*dz<.25f) {old_matched[i]=1;new_matched[i]=1;}
    else {ordered=false;break;}
  }
  if(!ordered) {
    std::fill(old_matched.begin(),old_matched.end(),0);std::fill(new_matched.begin(),new_matched.end(),0);
    for(unsigned i=0;i<s.light_bounds.size();++i)for(unsigned j=0;j<lights.size();++j) {
      if(new_matched[j])continue;
      const auto dx=lights[j].position_range[0]-s.light_bounds[i][0],dy=lights[j].position_range[1]-s.light_bounds[i][1],
        dz=lights[j].position_range[2]-s.light_bounds[i][2];
      if(dx*dx+dy*dy+dz*dz<.25f) {old_matched[i]=1;new_matched[j]=1;break;}
    }
  }
  const bool light_added=std::any_of(new_matched.begin(),new_matched.end(),[](char m){return !m;});
  const bool light_removed=std::any_of(old_matched.begin(),old_matched.end(),[](char m){return !m;});
  // Small flame flicker never wakes: GI holds the smoothed value while direct
  // light still dances every frame. A forced gentle refresh bounds the drift.
  const bool flicker_wake=s.time_seconds-s.light_consumed_seconds>=.5;
  if(light_added || light_removed || (s.light_revision!=r.local_lighting.light_revision && flicker_wake)) {
    const float margin=s.config.spacing;
    const auto wake=[&](const std::array<float,4>& bounds,bool hard) {
      const float reach=bounds[3];
      ddgi_invalidate(s,{bounds[0]-reach,bounds[1]-reach,bounds[2]-reach},
        {bounds[0]+reach,bounds[1]+reach,bounds[2]+reach},margin,true,hard);
    };
    // Reject the full old influence before recursive tracing, not just its core.
    // Additions blend their own reach; untouched lights are left alone so one
    // torch click cannot re-trace every other torch region in the volume.
    const bool shape_changed=light_added||light_removed;
    for(unsigned i=0;i<s.light_bounds.size();++i)if(!old_matched[i])wake(s.light_bounds[i],true);
    s.light_bounds.clear();
    for(unsigned j=0;j<lights.size();++j) {
      const auto& light=lights[j];
      float reach=std::max(light.position_range[3],0.f);
      if(light.axis_v_type[3]==2.f)
        reach+=float(std::sqrt(light.axis_u_inner[0]*light.axis_u_inner[0]+light.axis_u_inner[1]*light.axis_u_inner[1]+
          light.axis_u_inner[2]*light.axis_u_inner[2])+std::sqrt(light.axis_v_type[0]*light.axis_v_type[0]+
          light.axis_v_type[1]*light.axis_v_type[1]+light.axis_v_type[2]*light.axis_v_type[2]));
      s.light_bounds.push_back({light.position_range[0],light.position_range[1],light.position_range[2],reach});
      if(!new_matched[j]||!shape_changed)wake(s.light_bounds.back(),false);
    }
    s.light_revision=r.local_lighting.light_revision;
    s.light_consumed_seconds=s.time_seconds;
    if(light_added || light_removed)s.burst_frames=std::max(s.burst_frames,8u);
  } else if(s.light_revision!=r.local_lighting.light_revision) {
    // Smoothed flicker step: refresh the baseline without waking any probe.
    s.light_revision=r.local_lighting.light_revision;
  }
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
      std::max(s.config.spacing*3.f,4.f));
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
  if(!s.selected_intervals.empty() && !world_rhi_ok(commands->uploadBufferData(s.history_intervals[r.active_frame],0,
      s.selected_intervals.size()*sizeof(float),s.selected_intervals.data())))return false;
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
  s.available=enabled && coarse>0;s.cell_centered=false;
  s.config=ddgi_volume_config(s.base_config,coarse,false,s.env_spacing);
  if(enabled && voxel>0) {
    if(!s.fine_volume)s.fine_volume=std::make_unique<DDGISystem>();
    s.fine_volume->available=true;s.fine_volume->cell_centered=true;
    s.fine_volume->config=ddgi_volume_config(s.base_config,voxel,true,1);
  } else s.fine_volume.reset();
  const auto reset=[](DDGISystem& s) {
    s.stats=DDGIStats{};s.control_data.clear();s.last_updates.clear();
    s.last_update_times.clear();s.selected_intervals.clear();s.time_seconds=s.light_consumed_seconds=0;s.update_clock={};
    s.frame_seconds=1./60;s.budget_credit=0;
    s.dirty.clear();s.selected.clear();s.occupancy.clear();
    s.initialized=false;s.controls_dirty=true;s.seed_needed=true;s.frame=0;
    s.scene_revision=s.light_revision=0;s.light_bounds.clear();
    s.debug_boxes={};s.debug_box_cursor=0;
    s.dispatch_capacity=0;s.burst_frames=8;
    s.classified_origin={std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::max(),
      std::numeric_limits<std::int32_t>::max()};
    s.classified_ignore_voxel=s.classified_origin;
    s.classified_revision=~0ull;s.classified_ignore=false;
    s.sky_tops.clear();s.sky_tops_revision=~0ull;
    ddgi_clear_ignore(s);
  };
  reset(s);if(s.fine_volume)reset(*s.fine_volume);
  if(!initialize_volume(r,s) || (s.fine_volume && !initialize_volume(r,*s.fine_volume)))return false;
  s.scene_revision=r.scene_changes.revision();s.light_revision=r.local_lighting.light_revision;
  if(s.fine_volume) {
    s.fine_volume->scene_revision=s.scene_revision;s.fine_volume->light_revision=s.light_revision;
  }
  return true;
}
bool world_ddgi_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.ddgi;
  return bind_volume(r,root,s,false,true) &&
    bind_volume(r,root,s.fine_volume?*s.fine_volume:s,true,bool(s.fine_volume));
}
bool world_ddgi_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.ddgi;s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  if(!r.ray_enabled || (!s.available && !s.fine_volume))return true;
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
    std::printf("world_ddgi frame=%llu scheduled_probes=%u ray_budget=%u invalidated=%u bytes=%llu probes=%u fine_probes=%u\n",
      static_cast<unsigned long long>(s.frame),s.stats.updated_probes+fine.updated_probes,s.stats.scheduled_rays+fine.scheduled_rays,
      s.stats.invalidated_probes+fine.invalidated_probes,static_cast<unsigned long long>(s.stats.bytes+fine.bytes),s.stats.probe_count,fine.probe_count);
  }
  return true;
}
}
