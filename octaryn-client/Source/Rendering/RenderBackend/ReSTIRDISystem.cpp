#include "WorldRendererInternal.h"
#include "ReSTIRDISystem.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
void memory_stats(ReSTIRDISystem& s) {
  s.gpu_bytes=std::uint64_t(s.width)*s.height*8;
  for(auto* value:{s.light_buffer.get(),s.initial.get(),s.temporal.get(),s.spatial.get(),s.history.get(),
      s.surfaces.get(),s.counters.get(),s.tile_counts.get(),s.tile_lights.get()})if(value)s.gpu_bytes+=value->getDesc().size;
}
bool buffer(WorldRenderer& r,Slang::ComPtr<rhi::IBuffer>& out,std::uint64_t count,unsigned stride) {
  rhi::BufferDesc d{};d.size=std::max<std::uint64_t>(count,1)*stride;d.elementSize=stride;
  d.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  d.defaultState=rhi::ResourceState::ShaderResource;
  return world_rhi_ok(r.device->createBuffer(d,nullptr,out.writeRef()));
}
bool resources(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.restir;unsigned width=unsigned(r.render_width()),height=unsigned(r.render_height());
  if(s.width==width && s.height==height && s.output)return true;
  s.history_valid=false;s.width=width;s.height=height;
  rhi::TextureDesc d{};d.size={width,height,1};d.format=rhi::Format::RGBA16Float;d.label="restir_direct_local";
  d.usage=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
  d.defaultState=rhi::ResourceState::ShaderResource;
  s.output_view.setNull();s.output.setNull();
  if(!world_rhi_ok(r.device->createTexture(d,nullptr,s.output.writeRef())) ||
      !world_rhi_ok(s.output->getDefaultView(s.output_view.writeRef())))return false;
  float zero[4]{};commands->clearTextureFloat(s.output,{0,1,0,1},zero);
  commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
  // Allocate reservoirs on demand: no local lights costs only the composition texture.
  s.initial.setNull();s.temporal.setNull();s.spatial.setNull();s.history.setNull();s.surfaces.setNull();
  s.tile_counts.setNull();s.tile_lights.setNull();
  s.reservoir_count=0;s.gpu_bytes=std::uint64_t(width)*height*8;
  return true;
}
bool reservoir_resources(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.restir;if(s.initial)return true;
  const auto count=std::uint64_t(s.width)*s.height;
  if(!buffer(r,s.initial,count,32)||!buffer(r,s.temporal,count,32)||!buffer(r,s.spatial,count,32)||
      !buffer(r,s.history,count,32)||!buffer(r,s.surfaces,count,32)||!buffer(r,s.counters,4,4))return false;
  s.reservoir_count=count;s.gpu_bytes=count*(8+32*4+32)+16;
  commands->clearBuffer(s.history);commands->clearBuffer(s.surfaces);
  commands->setBufferState(s.history,rhi::ResourceState::ShaderResource);
  commands->setBufferState(s.surfaces,rhi::ResourceState::ShaderResource);return true;
}
template<class T> bool data(rhi::ShaderCursor c,const char* name,const T& value) {
  auto field=c[name];return !field.isValid() || world_rhi_ok(field.setData(&value,sizeof(value)));
}
bool binding(rhi::ShaderCursor c,const char* name,rhi::IBuffer* value) {
  auto field=c[name];return !field.isValid() || world_rhi_ok(field.setBinding(rhi::Binding(value)));
}
bool common(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.restir;auto& hdr=r.target().hdr;rhi::ShaderCursor c(root);
  const float eye[4]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
  const unsigned extent[4]={s.width,s.height,unsigned(r.frames),unsigned(s.lights.size())};
  const float reuse[4]={std::clamp(s.settings.position_threshold,.001f,2.f),std::clamp(s.settings.normal_threshold,0.f,1.f),
      std::clamp(s.settings.spatial_radius,0.f,64.f),float(std::clamp(s.settings.history_limit,1u,256u))};
  const unsigned settings[4]={std::clamp(s.settings.candidates,1u,32u),s.settings.spatial?std::min(s.settings.spatial_samples,8u):0u,
      s.history_valid && s.settings.temporal?1u:0u,s.settings.debug};
  if(!binding(c,"localLights",s.light_buffer)||!data(c,"eye",eye)||!data(c,"extent",extent)||
      !data(c,"reuseSettings",reuse)||!data(c,"restirSettings",settings))return false;
  const char* names[]={"colors","positions","voxels","materials"};
  for(unsigned i=0;i<4;++i)if(c[names[i]].isValid() && !world_rhi_ok(c[names[i]].setBinding(hdr.views[i])))return false;
  return true;
}
template<class Bind> bool dispatch(WorldRenderer& r,rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,Bind bind) {
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(pipeline);bool ok=root && common(r,root) && bind(root);
  if(ok)pass->dispatchCompute((r.restir.width+7)/8,(r.restir.height+7)/8,1);
  pass->end();return ok;
}
}
bool open_world_renderer_set_lights(WorldRenderer* r,const WorldLocalLight* lights,std::uint32_t count) {
  if(!r || count>65536 || (count && !lights))return false;
  std::vector<WorldLocalLight> accepted;accepted.reserve(count);
  for(std::uint32_t i=0;i<count;++i) {
    auto light=lights[i];
    for(const auto& values:{light.position_range,light.color_intensity,light.direction_outer,light.axis_u_inner,light.axis_v_type})
      for(float value:values)if(!std::isfinite(value))return false;
    const float type=light.axis_v_type[3];
    if(type!=0 && type!=1 && type!=2)return false;
    if(light.position_range[3]<=0 || light.color_intensity[3]<0 ||
        light.color_intensity[0]<0 || light.color_intensity[1]<0 || light.color_intensity[2]<0)return false;
    if(type!=0) {
      auto& d=light.direction_outer;float length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
      if(length<1e-5f)return false;for(unsigned a=0;a<3;++a)d[a]/=length;
    }
    if(type==1 && (light.direction_outer[3]<-1 || light.axis_u_inner[3]>1 || light.axis_u_inner[3]<=light.direction_outer[3]))return false;
    if(type==2) {
      const auto& u=light.axis_u_inner;const auto& v=light.axis_v_type;
      const float area=std::hypot(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]);
      if(area<1e-5f)return false;
    }
    accepted.push_back(light);
  }
  auto& state=r->block_lights;
  if(state.explicit_lights.size()==accepted.size() && (accepted.empty() || std::memcmp(state.explicit_lights.data(),accepted.data(),accepted.size()*sizeof(WorldLocalLight))==0))return true;
  state.explicit_lights=std::move(accepted);state.dirty=true;world_block_lights_update(*r);return true;
}
bool world_restir_initialize(WorldRenderer& r) {
  auto& s=r.restir;
  if(!buffer(r,s.light_buffer,1,sizeof(WorldLocalLight)) || !initialize_local_shadows(r))return false;
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ReSTIRInitial.slang","main",s.initial_pipeline)||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ReSTIRTemporal.slang","main",s.temporal_pipeline)||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ReSTIRSpatial.slang","main",s.spatial_pipeline)||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ReSTIRFallback.slang","main",s.fallback_pipeline)||
      !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ClusteredLocalLights.slang","main",s.tile_pipeline))return false;
  return !world_ray_available(r) || create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ReSTIRVisibility.slang","main",s.visibility_pipeline);
}
bool world_restir_prepare_lights(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.restir;
  if(s.uploaded_revision==s.light_revision || s.lights.empty())return true;
  if(!buffer(r,s.light_buffer,s.lights.size(),sizeof(WorldLocalLight)) ||
      !world_rhi_ok(commands->uploadBufferData(s.light_buffer,0,s.lights.size()*sizeof(WorldLocalLight),s.lights.data())))return false;
  commands->setBufferState(s.light_buffer,rhi::ResourceState::ShaderResource);
  s.uploaded_revision=s.light_revision;s.history_valid=false;return true;
}
bool world_restir_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.restir;if(!resources(r,commands))return false;
  if(s.lights.empty()) {
    if(s.active) {
      float zero[4]{};commands->clearTextureFloat(s.output,{0,1,0,1},zero);
      commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
      if(s.counters)commands->clearBuffer(s.counters);
    }
    s.active=false;s.history_valid=false;s.visibility_ray_budget=0;memory_stats(s);return true;
  }
  if(!reservoir_resources(r,commands))return false;
  if(!world_restir_prepare_lights(r,commands))return false;
  const auto revision=r.scene_changes.revision();
  if(revision!=s.scene_revision || (r.temporal.mode && r.temporal.reset))s.history_valid=false;
  s.scene_revision=revision;
  const auto view=temporal_view(r.temporal.camera,int(s.width),int(s.height));
  float camera_delta=0;for(unsigned i=0;i<3;++i) {const float delta=view.position[i]-s.previous.position[i];camera_delta+=delta*delta;}
  if(camera_delta>64 || std::abs(view.projection[1]-s.previous.projection[1])>.01f)s.history_valid=false;
  commands->setBufferState(s.light_buffer,rhi::ResourceState::ShaderResource);
  commands->clearBuffer(s.counters);commands->setBufferState(s.counters,rhi::ResourceState::UnorderedAccess);
  const bool rt=r.ray_enabled && world_ray_available(r) && s.visibility_pipeline && r.lighting_settings.quality>=LightingQuality::High;
  r.lighting_profile.begin_pass(commands,LightingPass::ReSTIRInitial);
  if(!rt && (!update_local_shadows(r,commands) || !world_local_clusters(r,commands)))return false;
  if(!dispatch(r,commands,s.initial_pipeline,[&](rhi::IShaderObject* root){return binding(rhi::ShaderCursor(root),"result",s.initial);}))return false;
  r.lighting_profile.mark(commands,LightingPass::ReSTIRInitial);
  commands->setBufferState(s.initial,rhi::ResourceState::ShaderResource);
  commands->setBufferState(s.history,rhi::ResourceState::ShaderResource);
  commands->setBufferState(s.surfaces,rhi::ResourceState::ShaderResource);
  r.lighting_profile.begin_pass(commands,LightingPass::ReSTIRTemporal);
  if(!dispatch(r,commands,s.temporal_pipeline,[&](rhi::IShaderObject* root){
    rhi::ShaderCursor c(root);return binding(c,"initial",s.initial)&&binding(c,"history",s.history)&&binding(c,"surfaceHistory",s.surfaces)&&binding(c,"result",s.temporal)&&
      data(c,"previousPosition",s.previous.position)&&data(c,"previousRight",s.previous.right)&&data(c,"previousUp",s.previous.up)&&
      data(c,"previousForward",s.previous.forward)&&data(c,"previousProjection",s.previous.projection)&&data(c,"previousJitter",s.previous_jitter);
  }))return false;
  r.lighting_profile.mark(commands,LightingPass::ReSTIRTemporal);
  commands->setBufferState(s.temporal,rhi::ResourceState::ShaderResource);
  r.lighting_profile.begin_pass(commands,LightingPass::ReSTIRSpatial);
  if(!dispatch(r,commands,s.spatial_pipeline,[&](rhi::IShaderObject* root){
    rhi::ShaderCursor c(root);return binding(c,"initial",s.temporal)&&binding(c,"result",s.spatial);
  }))return false;
  r.lighting_profile.mark(commands,LightingPass::ReSTIRSpatial);
  commands->setBufferState(s.spatial,rhi::ResourceState::ShaderResource);
  s.visibility_ray_budget=rt?s.reservoir_count:0;
  r.lighting_profile.begin_pass(commands,LightingPass::LocalVisibility);
  if(!dispatch(r,commands,rt?s.visibility_pipeline.get():s.fallback_pipeline.get(),[&](rhi::IShaderObject* root){
    rhi::ShaderCursor c(root);
    const unsigned tiles[4]={(s.width+15)/16,(s.height+15)/16,s.tile_capacity,16};
    if(!rt && (!binding(c,"localTileCounts",s.tile_counts)||!binding(c,"localTileLights",s.tile_lights)||!data(c,"localTileSettings",tiles)||!bind_local_shadows(r,root)))return false;
    return binding(c,"initial",s.temporal)&&binding(c,"selected",s.spatial)&&binding(c,"history",s.history)&&binding(c,"surfaceHistory",s.surfaces)&&
      binding(c,"localCounters",s.counters)&&world_rhi_ok(c["localLighting"].setBinding(s.output_view))&&(!rt || (world_ray_bind(r,root)&&bind_world_atlas(r.atlas,root)));
  }))return false;
  r.lighting_profile.mark(commands,LightingPass::LocalVisibility);
  commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
  s.previous=view;s.previous_jitter={r.draw_uniforms[7],r.draw_uniforms[11],0,0};
  s.history_valid=true;s.active=true;memory_stats(s);return true;
}
bool world_restir_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  return root && r.restir.output_view && world_rhi_ok(rhi::ShaderCursor(root)["localLighting"].setBinding(r.restir.output_view));
}
}
