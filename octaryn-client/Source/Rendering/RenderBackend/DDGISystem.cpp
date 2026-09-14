#include "WorldRendererInternal.h"
#include "DDGISystem.h"
#include "SceneChanges.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

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
bool dispatch(WorldRenderer& r,rhi::ICommandEncoder* commands,bool trace) {
  auto& s=r.ddgi;auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(trace?s.trace.get():s.update.get());
  const unsigned count=static_cast<unsigned>(s.selected.size());
  bool ok=root && world_ddgi_bind(r,root) && bind(root,"ddgiSelection",s.selections[r.active_frame]) &&
    bind(root,"ddgiRays",s.rays) && uniform(root,"ddgiUpdateCount",count);
  if(ok && trace) {
    const std::array<float,4> sun{-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
    const std::array<float,4> sky{r.lighting.visual_sky_visibility,r.lighting.ambient_strength,
      r.sky.twilight_celestial_time[0],r.sky.twilight_celestial_time[1]};
    const unsigned lights=static_cast<unsigned>(r.restir.lights.size());
    ok=world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && uniform(root,"ddgiSun",sun) && uniform(root,"ddgiSky",sky) &&
      bind(root,"localLights",r.restir.light_buffer) && uniform(root,"ddgiLightCount",lights);
  }
  if(ok)pass->dispatchCompute(trace?(count*s.config.rays+63)/64:count,1,1);
  pass->end();return ok;
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
  c.rays=unsigned(setting("OCTARYN_DDGI_RAYS",64,32,512));
  c.budget=unsigned(setting("OCTARYN_DDGI_BUDGET",64,1,512));
  c.irradiance_resolution=unsigned(setting("OCTARYN_DDGI_IRRADIANCE_RESOLUTION",6,2,16));
  c.visibility_resolution=unsigned(setting("OCTARYN_DDGI_VISIBILITY_RESOLUTION",8,2,16));
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
  std::printf("world_ddgi enabled=%u probes=%u rays_per_probe=%u update_budget=%u spacing=%.2f bytes=%llu\n",
    unsigned(s.available),s.stats.probe_count,c.rays,c.budget,c.spacing,static_cast<unsigned long long>(s.stats.bytes));
  return true;
}
bool world_ddgi_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  if(!root)return false;
  auto& s=r.ddgi;const auto& c=s.config;
  const std::array<unsigned,4> grid{c.counts[0],c.counts[1],c.counts[2],s.available&&r.ray_enabled?1u:0u};
  const std::array<int,4> origin{s.origin[0],s.origin[1],s.origin[2],0};
  const std::array<float,4> parameters{c.spacing,c.hysteresis,c.max_distance,std::min(.2f,c.spacing*.05f)};
  const std::array<unsigned,4> frame{static_cast<unsigned>(s.frame),c.rays,c.irradiance_resolution,c.visibility_resolution};
  return bind(root,"ddgiControls",s.controls) && bind(root,"ddgiProbes",s.probes) &&
    bind(root,"ddgiIrradiance",s.irradiance) && bind(root,"ddgiDistance",s.distance) &&
    uniform(root,"ddgiGrid",grid) && uniform(root,"ddgiOrigin",origin) &&
    uniform(root,"ddgiParameters",parameters) && uniform(root,"ddgiFrame",frame);
}
bool world_ddgi_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.ddgi;s.stats.updated_probes=s.stats.scheduled_rays=s.stats.invalidated_probes=0;
  if(!s.available || !r.ray_enabled)return true;
  if(!world_restir_prepare_lights(r,commands))return false;
  ++s.frame;
  if(s.light_revision!=r.restir.light_revision) {
    ddgi_invalidate(s,{-1e30f,-1e30f,-1e30f},{1e30f,1e30f,1e30f});
    s.light_revision=r.restir.light_revision;
  }
  if(!r.scene_changes.for_each_since(s.scene_revision,[&](const SceneChange& change) {
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
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGITrace);
  if(!dispatch(r,commands,true))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGITrace);
  commands->globalBarrier();
  r.lighting_profile.begin_pass(commands,LightingPass::DDGIUpdate);
  if(!dispatch(r,commands,false))return false;
  r.lighting_profile.mark(commands,LightingPass::DDGIUpdate);
  commands->globalBarrier();
  if(r.frames%120==0)std::printf("world_ddgi frame=%llu scheduled_probes=%u ray_budget=%u invalidated=%u bytes=%llu\n",
    static_cast<unsigned long long>(s.frame),s.stats.updated_probes,s.stats.scheduled_rays,s.stats.invalidated_probes,
    static_cast<unsigned long long>(s.stats.bytes));
  return true;
}
}
