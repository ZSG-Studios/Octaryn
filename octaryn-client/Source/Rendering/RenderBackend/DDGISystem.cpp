#include "WorldRendererInternal.h"
#include "DDGISystem.h"
#include "DDGIOccupancy.h"
#include "SceneChanges.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
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
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(trace?s.trace.get():s.update.get());
  const unsigned count=static_cast<unsigned>(s.selected.size());
  // Both traces sample the same previous-frame hierarchy before either update.
  auto* other=s.cell_centered?&r.ddgi:r.ddgi.fine_volume.get();
  bool ok=root && bind_volume(r,root,s,false,true) &&
    bind_volume(r,root,other?*other:s,true,trace&&other!=nullptr) &&
    bind(root,"ddgiSelection",s.selections[r.active_frame]) &&
    bind(root,"ddgiRays",s.rays) && uniform(root,"ddgiUpdateCount",count);
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
  if(!allocate(r,s.controls,count,sizeof(DDGIControl),"ddgi_grid_controls") ||
     !allocate(r,s.probes,count,sizeof(DDGIProbe),"ddgi_probe_state") ||
     !allocate(r,s.irradiance,count*c.irradiance_resolution*c.irradiance_resolution,16,"ddgi_irradiance") ||
     !allocate(r,s.distance,count*c.visibility_resolution*c.visibility_resolution,8,"ddgi_distance_moments") ||
     !allocate(r,s.variability,count,4,"ddgi_variability") ||
     !allocate(r,s.rays,c.budget*c.rays,32,"ddgi_ray_results"))return false;
  for(auto& selection:s.selections)if(!allocate(r,selection,c.budget,4,"ddgi_probe_selection"))return false;
  s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count,true);
  s.stats.probe_count=s.available?count:0;
  for(auto* resource:{s.controls.get(),s.probes.get(),s.irradiance.get(),s.distance.get(),s.rays.get(),
      s.selections[0].get(),s.selections[1].get()})s.stats.bytes+=resource->getDesc().size;
  if(s.available && (!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGITrace.slang","main",s.trace) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGIUpdate.slang","main",s.update) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGISeed.slang","main",s.seed)))return false;
  std::printf("world_ddgi enabled=%u probes=%u rays_per_probe=%u update_budget=%u spacing=%.2f bytes=%llu fine=%u\n",
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
  // Placement/removal wakes immediately; intensity-only revisions (flicker) are
  // rate-limited so a 20 Hz fire cannot wake its region every frame.
  const bool lightsReshaped=r.local_lighting.lights.size()!=s.light_bounds.size();
  if(s.light_revision!=r.local_lighting.light_revision &&
     (lightsReshaped || s.frame-s.light_consumed_frame>=10)) {
    // Light edits wake only the probes inside their influence bounds — both the
    // previous and the new publication, so moved/removed lights refresh the
    // region they used to touch instead of the whole volume.
    const float margin=std::max(s.config.spacing,4.f);
    const auto wake=[&](const std::array<float,4>& bounds) {
      ddgi_invalidate(s,{bounds[0]-bounds[3],bounds[1]-bounds[3],bounds[2]-bounds[3]},
        {bounds[0]+bounds[3],bounds[1]+bounds[3],bounds[2]+bounds[3]},margin);
    };
    for(const auto& bounds:s.light_bounds)wake(bounds);
    s.light_bounds.clear();
    for(const auto& light:r.local_lighting.lights) {
      float reach=std::max(light.position_range[3],0.f);
      if(light.axis_v_type[3]==2.f)
        reach+=float(std::sqrt(light.axis_u_inner[0]*light.axis_u_inner[0]+light.axis_u_inner[1]*light.axis_u_inner[1]+
          light.axis_u_inner[2]*light.axis_u_inner[2])+std::sqrt(light.axis_v_type[0]*light.axis_v_type[0]+
          light.axis_v_type[1]*light.axis_v_type[1]+light.axis_v_type[2]*light.axis_v_type[2]));
      const std::array<float,4> bounds{light.position_range[0],light.position_range[1],light.position_range[2],reach};
      s.light_bounds.push_back(bounds);wake(bounds);
    }
    s.light_revision=r.local_lighting.light_revision;
    s.light_consumed_frame=s.frame;
  }
  ddgi_follow_opening(r,s);
  if(!r.scene_changes.for_each_since(s.scene_revision,[&](const SceneChange& change) {
    const bool opening=ddgi_ignore_covers(s,change.x,change.z);
    // The hole probe is seeded from neighbors while the voxel is still solid.
    // Wait for other edits until the replacement acceleration structure exists.
    if(change.kind==SceneChangeKind::Added)return;
    if(change.kind==SceneChangeKind::Modified && !opening)return;
    if(change.kind==SceneChangeKind::AccelerationReady && opening)ddgi_clear_ignore(s);
    const float x=float(change.x)*32,z=float(change.z)*32;
    ddgi_invalidate(s,{x,float(change.min_y),z},{x+32,float(change.min_y+change.height),z+32},
      std::max(s.config.spacing*3.f,4.f));
  }))ddgi_invalidate(s,{-1e30f,-1e30f,-1e30f},{1e30f,1e30f,1e30f});
  s.scene_revision=r.scene_changes.revision();
  ddgi_classify_occupancy(r,s);
  ddgi_schedule(s,{r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2]});
  if(s.controls_dirty) {
    if(!world_rhi_ok(commands->uploadBufferData(s.controls,0,s.control_data.size()*sizeof(DDGIControl),s.control_data.data())))return false;
    s.controls_dirty=false;
  }
  if(!world_rhi_ok(commands->uploadBufferData(s.selections[r.active_frame],0,s.selected.size()*sizeof(unsigned),s.selected.data())))return false;
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
  s.env_spacing=c.spacing;
  settings.ddgi_coarse_radius=s.available?unsigned(c.counts[0]*c.spacing/2):0;
  settings.ddgi_voxel_radius=s.available?6u:0;
  return world_ddgi_reconfigure(r);
}
// Size both volumes from the current block radii, reset CPU/GPU-side state and
// reallocate. Callers flush the GPU first; ComPtr overwrites release the old
// buffers once no in-flight frame references them.
bool world_ddgi_reconfigure(WorldRenderer& r) {
  auto& s=r.ddgi;auto& c=s.config;
  const unsigned voxel=r.lighting_settings.ddgi_voxel_radius,coarse=r.lighting_settings.ddgi_coarse_radius;
  s.available=world_ray_available(r) && coarse>0;
  if(coarse>0) {
    // Reach the requested radius within the 64-cell toroidal limit: widen the
    // spacing past 256 blocks instead of dropping distant coverage.
    c.spacing=std::max(s.env_spacing,float(int((coarse*2+63)/64)));
    const unsigned side=std::clamp(unsigned(coarse*2/c.spacing),2u,64u);
    c.counts={side,12,side};
    c.budget=std::clamp(side*4,64u,256u);
    c.max_distance=std::max(96.f,c.spacing*12.f);
  }
  if(voxel>0 && world_ray_available(r)) {
    if(!s.fine_volume)s.fine_volume=std::make_unique<DDGISystem>();
    auto& fine=*s.fine_volume;
    fine.available=true;fine.cell_centered=true;fine.config=c;
    const unsigned side=std::clamp(voxel*2,2u,64u);
    fine.config.counts={side,std::clamp(voxel*2,2u,16u),side};
    fine.config.spacing=1;
    fine.config.budget=std::clamp(voxel*16,64u,256u);
  } else s.fine_volume.reset();
  const auto reset=[](DDGISystem& volume) {
    volume.stats=DDGIStats{};volume.control_data.clear();volume.last_updates.clear();
    volume.dirty.clear();volume.selected.clear();volume.occupancy.clear();
    volume.initialized=false;volume.controls_dirty=true;volume.frame=0;
    volume.scene_revision=volume.light_revision=0;volume.light_bounds.clear();
    volume.light_consumed_frame=0;volume.debug_boxes={};volume.debug_box_cursor=0;
  };
  reset(s);if(s.fine_volume)reset(*s.fine_volume);
  if(!initialize_volume(r,s) || (s.fine_volume && !initialize_volume(r,*s.fine_volume)))return false;
  s.scene_revision=r.scene_changes.revision();s.light_revision=r.local_lighting.light_revision;
  if(s.fine_volume) {
    s.fine_volume->scene_revision=s.scene_revision;
    s.fine_volume->light_revision=s.light_revision;
  }
  return true;
}
bool world_ddgi_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.ddgi;
  return bind_volume(r,root,s,false,true) && bind_volume(r,root,s.fine_volume?*s.fine_volume:s,true,bool(s.fine_volume));
}
bool world_ddgi_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.ddgi;s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  const bool coarse=s.available && r.ray_enabled;
  if(!coarse && !s.fine_volume)return true;
  if((coarse && !prepare_volume(r,s,commands)) ||
     (s.fine_volume && !prepare_volume(r,*s.fine_volume,commands)))return false;
  commands->globalBarrier();
  if((coarse && !dispatch_seed(r,s,commands)) || (s.fine_volume && !dispatch_seed(r,*s.fine_volume,commands)))return false;
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGITrace);
  if((coarse && !dispatch(r,s,commands,true)) || (s.fine_volume && !dispatch(r,*s.fine_volume,commands,true)))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGITrace);
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGIUpdate);
  if((coarse && !dispatch(r,s,commands,false)) || (s.fine_volume && !dispatch(r,*s.fine_volume,commands,false)))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGIUpdate);
  commands->globalBarrier();
  if(r.frames%120==0) {
    const DDGIStats fine=s.fine_volume?s.fine_volume->stats:DDGIStats{};
    std::printf("world_ddgi frame=%llu scheduled_probes=%u ray_budget=%u invalidated=%u bytes=%llu fine_probes=%u\n",
      static_cast<unsigned long long>(s.frame),s.stats.updated_probes+fine.updated_probes,
      s.stats.scheduled_rays+fine.scheduled_rays,s.stats.invalidated_probes+fine.invalidated_probes,
      static_cast<unsigned long long>(s.stats.bytes+fine.bytes),fine.probe_count);
  }
  return true;
}
}
