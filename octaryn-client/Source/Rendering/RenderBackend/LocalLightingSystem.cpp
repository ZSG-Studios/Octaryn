#include "WorldRendererInternal.h"
#include "LocalLightingSystem.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
void memory_stats(LocalLightingSystem& s) {
  s.gpu_bytes=std::uint64_t(s.width)*s.height*8;
  for(auto* value:{s.light_buffer.get(),s.counters.get(),s.tile_counts.get(),s.tile_lights.get()})
    if(value)s.gpu_bytes+=value->getDesc().size;
}
bool buffer(WorldRenderer& r,Slang::ComPtr<rhi::IBuffer>& out,std::uint64_t count,unsigned stride) {
  rhi::BufferDesc d{};d.size=std::max<std::uint64_t>(count,1)*stride;d.elementSize=stride;
  d.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  d.defaultState=rhi::ResourceState::ShaderResource;
  return world_rhi_ok(r.device->createBuffer(d,nullptr,out.writeRef()));
}
bool resources(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.local_lighting;const unsigned width=unsigned(r.render_width()),height=unsigned(r.render_height());
  if(s.width==width && s.height==height && s.output)return true;
  s.width=width;s.height=height;s.active=false;
  rhi::TextureDesc d{};d.size={width,height,1};d.format=rhi::Format::RGBA16Float;d.label="local_direct_lighting";
  d.usage=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination|rhi::TextureUsage::CopySource;
  d.defaultState=rhi::ResourceState::ShaderResource;
  s.output_view.setNull();s.output.setNull();
  if(!world_rhi_ok(r.device->createTexture(d,nullptr,s.output.writeRef())) ||
      !world_rhi_ok(s.output->getDefaultView(s.output_view.writeRef())))return false;
  float zero[4]{};commands->clearTextureFloat(s.output,{0,1,0,1},zero);
  commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
  s.tile_counts.setNull();s.tile_lights.setNull();s.shaded_pixel_count=0;
  return true;
}
template<class T> bool data(rhi::ShaderCursor c,const char* name,const T& value) {
  auto field=c[name];return !field.isValid() || world_rhi_ok(field.setData(&value,sizeof(value)));
}
bool binding(rhi::ShaderCursor c,const char* name,rhi::IBuffer* value) {
  auto field=c[name];return !field.isValid() || world_rhi_ok(field.setBinding(rhi::Binding(value)));
}
bool dispatch(WorldRenderer& r,rhi::ICommandEncoder* commands,bool rt) {
  auto& s=r.local_lighting;auto& hdr=r.target().hdr;
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(rt?s.direct_pipeline.get():s.fallback_pipeline.get());
  bool ok=root!=nullptr;
  if(ok) {
    rhi::ShaderCursor c(root);
    const float eye[4]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
    const unsigned extent[4]={s.width,s.height,unsigned(r.frames),unsigned(s.lights.size())};
    const unsigned tiles[4]={(s.width+15)/16,(s.height+15)/16,s.tile_capacity,16};
    const unsigned settings[4]={s.settings.debug,0,0,0};
    ok=binding(c,"localLights",s.light_buffer)&&binding(c,"localCounters",s.counters)&&
      binding(c,"localTileCounts",s.tile_counts)&&binding(c,"localTileLights",s.tile_lights)&&
      data(c,"eye",eye)&&data(c,"extent",extent)&&data(c,"localTileSettings",tiles)&&data(c,"localSettings",settings)&&
      world_rhi_ok(c["localLighting"].setBinding(s.output_view));
    const char* names[]={"colors","positions","voxels","materials"};
    for(unsigned i=0;ok&&i<4;++i)ok=world_rhi_ok(c[names[i]].setBinding(hdr.views[i]));
    if(ok)ok=rt?(world_ray_bind(r,root)&&bind_world_atlas(r.atlas,root)):bind_local_shadows(r,root);
  }
  if(ok)pass->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
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
bool world_local_lighting_initialize(WorldRenderer& r) {
  auto& s=r.local_lighting;
  if(!buffer(r,s.light_buffer,1,sizeof(WorldLocalLight)) || !buffer(r,s.counters,4,4) ||
     !initialize_local_shadows(r))return false;
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/LocalDirect.slang","main",s.fallback_pipeline)||
     !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ClusteredLocalLights.slang","main",s.tile_pipeline)||
     !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/ClusteredLocalSort.slang","main",s.tile_sort_pipeline))return false;
  return !world_ray_available(r) ||
      create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Lighting/LocalDirectRT.slang","main",s.direct_pipeline);
}
bool world_local_lighting_prepare(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.local_lighting;
  if(s.uploaded_revision==s.light_revision)return true;
  if(!s.lights.empty() && (!buffer(r,s.light_buffer,s.lights.size(),sizeof(WorldLocalLight)) ||
      !world_rhi_ok(commands->uploadBufferData(s.light_buffer,0,s.lights.size()*sizeof(WorldLocalLight),s.lights.data()))))return false;
  commands->setBufferState(s.light_buffer,rhi::ResourceState::ShaderResource);
  s.uploaded_revision=s.light_revision;return true;
}
bool world_local_lighting_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.local_lighting;if(!resources(r,commands))return false;
  commands->clearBuffer(s.counters);commands->setBufferState(s.counters,rhi::ResourceState::UnorderedAccess);
  if(s.lights.empty()) {
    if(s.active) {
      float zero[4]{};commands->clearTextureFloat(s.output,{0,1,0,1},zero);
      commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
    }
    s.active=false;s.visibility_ray_budget=0;s.shaded_pixel_count=0;memory_stats(s);return true;
  }
  if(!world_local_lighting_prepare(r,commands))return false;
  const bool rt=r.ray_enabled && world_ray_available(r) && s.direct_pipeline;
  r.lighting_profile.begin_pass(commands,LightingPass::LocalCull);
  if((!rt&&!update_local_shadows(r,commands)) || !world_local_clusters(r,commands))return false;
  r.lighting_profile.mark(commands,LightingPass::LocalCull);
  s.shaded_pixel_count=std::uint64_t(s.width)*s.height;
  std::uint64_t samples=0;for(const auto& light:s.lights)samples+=light.axis_v_type[3]==2?4:1;
  s.visibility_ray_budget=rt?s.shaded_pixel_count*samples:0;
  r.lighting_profile.begin_pass(commands,LightingPass::LocalShade);
  if(!dispatch(r,commands,rt))return false;
  r.lighting_profile.mark(commands,LightingPass::LocalShade);
  commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);
  s.active=true;memory_stats(s);return true;
}
bool world_local_lighting_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  auto* view=r.local_lighting.output_view.get();
  return root && view && world_rhi_ok(rhi::ShaderCursor(root)["localLighting"].setBinding(view));
}
}
