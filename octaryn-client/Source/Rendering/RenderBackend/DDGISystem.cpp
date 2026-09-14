#include "WorldRendererInternal.h"
#include "DDGISystem.h"
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
    ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && uniform(root,"ddgiSun",sun) && uniform(root,"ddgiSky",sky) &&
      bind(root,"localLights",r.local_lighting.light_buffer) && uniform(root,"ddgiLightCount",lights);
  }
  if(ok)pass->dispatchCompute(trace?(count*s.config.rays+63)/64:count,1,1);
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
     !allocate(r,s.rays,c.budget*c.rays,32,"ddgi_ray_results"))return false;
  for(auto& selection:s.selections)if(!allocate(r,selection,c.budget,4,"ddgi_probe_selection"))return false;
  s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count,true);
  s.stats.probe_count=s.available?count:0;
  for(auto* resource:{s.controls.get(),s.probes.get(),s.irradiance.get(),s.distance.get(),s.rays.get(),
      s.selections[0].get(),s.selections[1].get()})s.stats.bytes+=resource->getDesc().size;
  if(s.available && (!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGITrace.slang","main",s.trace) ||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGI/DDGIUpdate.slang","main",s.update)))return false;
  std::printf("world_ddgi enabled=%u probes=%u rays_per_probe=%u update_budget=%u spacing=%.2f bytes=%llu fine=%u\n",
    unsigned(s.available),s.stats.probe_count,c.rays,c.budget,c.spacing,static_cast<unsigned long long>(s.stats.bytes),unsigned(s.cell_centered));
  return true;
}
bool bind_volume(WorldRenderer& r,rhi::IShaderObject* root,DDGISystem& s,bool fine,bool enabled) {
  if(!root)return false;
  const auto& c=s.config;
  const std::array<unsigned,4> grid{c.counts[0],c.counts[1],c.counts[2],enabled&&s.available&&r.ray_enabled?1u:0u};
  const std::array<int,4> origin{s.origin[0],s.origin[1],s.origin[2],s.cell_centered?1:0};
  const std::array<float,4> fadeOrigin{s.fade_origin[0],s.fade_origin[1],s.fade_origin[2],0};
  const std::array<float,4> parameters{c.spacing,c.hysteresis,c.max_distance,std::min(.2f,c.spacing*.05f)};
  const std::array<unsigned,4> frame{static_cast<unsigned>(s.frame),c.rays,c.irradiance_resolution,c.visibility_resolution};
  const std::string prefix=fine?"ddgiFine":"ddgi";
  return bind(root,(prefix+"Controls").c_str(),s.controls) && bind(root,(prefix+"Probes").c_str(),s.probes) &&
    bind(root,(prefix+"Irradiance").c_str(),s.irradiance) && bind(root,(prefix+"Distance").c_str(),s.distance) &&
    uniform(root,(prefix+"Grid").c_str(),grid) && uniform(root,(prefix+"Origin").c_str(),origin) &&
    uniform(root,(prefix+"FadeOrigin").c_str(),fadeOrigin) && uniform(root,(prefix+"Parameters").c_str(),parameters) &&
    uniform(root,(prefix+"Frame").c_str(),frame);
}
bool prepare_volume(WorldRenderer& r,DDGISystem& s,rhi::ICommandEncoder* commands) {
  s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  ++s.frame;
  if(s.light_revision!=r.local_lighting.light_revision) {
    ddgi_invalidate(s,{-1e30f,-1e30f,-1e30f},{1e30f,1e30f,1e30f});
    s.light_revision=r.local_lighting.light_revision;
  }
  if(!r.scene_changes.for_each_since(s.scene_revision,[&](const SceneChange& change) {
    // Raster uploads still use the previous BLAS until its replacement is ready.
    // Start probe recovery when rays can actually see the edited geometry.
    if(change.kind==SceneChangeKind::Added || change.kind==SceneChangeKind::Modified)return;
    const float x=float(change.x)*32,z=float(change.z)*32;
    ddgi_invalidate(s,{x,float(change.min_y),z},{x+32,float(change.min_y+change.height),z+32});
  }))ddgi_invalidate(s,{-1e30f,-1e30f,-1e30f},{1e30f,1e30f,1e30f});
  s.scene_revision=r.scene_changes.revision();
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
  c.counts={unsigned(setting("OCTARYN_DDGI_COUNT_X",16,2,32)),unsigned(setting("OCTARYN_DDGI_COUNT_Y",8,2,32)),
    unsigned(setting("OCTARYN_DDGI_COUNT_Z",16,2,32))};
  c.spacing=setting("OCTARYN_DDGI_SPACING",4,.5f,32);
  c.hysteresis=setting("OCTARYN_DDGI_HYSTERESIS",.94f,0,.99f);
  c.max_distance=setting("OCTARYN_DDGI_MAX_DISTANCE",64,c.spacing,512);
  c.rays=unsigned(setting("OCTARYN_DDGI_RAYS",112,32,512));
  c.budget=unsigned(setting("OCTARYN_DDGI_BUDGET",64,1,512));
  c.irradiance_resolution=unsigned(setting("OCTARYN_DDGI_IRRADIANCE_RESOLUTION",6,2,16));
  c.visibility_resolution=unsigned(setting("OCTARYN_DDGI_VISIBILITY_RESOLUTION",8,2,16));
  if(!initialize_volume(r,s))return false;
  if(s.available && c.spacing>1.f) {
    s.fine_volume=std::make_unique<DDGISystem>();auto& fine=*s.fine_volume;
    fine.available=true;fine.cell_centered=true;fine.config=c;
    fine.config.counts={12,12,12};fine.config.spacing=1;
    fine.config.budget=std::min(64u,c.budget);
    if(!initialize_volume(r,fine))return false;
  }
  return true;
}
bool world_ddgi_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.ddgi;
  return bind_volume(r,root,s,false,true) && bind_volume(r,root,s.fine_volume?*s.fine_volume:s,true,bool(s.fine_volume));
}
bool world_ddgi_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.ddgi;s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  if(!s.available || !r.ray_enabled)return true;
  if(!prepare_volume(r,s,commands) ||
     (s.fine_volume && !prepare_volume(r,*s.fine_volume,commands)))return false;
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGITrace);
  if(!dispatch(r,s,commands,true) || (s.fine_volume && !dispatch(r,*s.fine_volume,commands,true)))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGITrace);
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGIUpdate);
  if(!dispatch(r,s,commands,false) || (s.fine_volume && !dispatch(r,*s.fine_volume,commands,false)))return false;
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
