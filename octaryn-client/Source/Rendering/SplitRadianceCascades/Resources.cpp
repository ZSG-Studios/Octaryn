#include "System.h"
#include "../RenderBackend/RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
namespace octaryn::client::rendering {
namespace {
bool buffer(rhi::IDevice* device,Slang::ComPtr<rhi::IBuffer>& out,unsigned count,unsigned stride,const char* label) {
  rhi::BufferDesc d{};d.size=std::uint64_t(count)*stride;d.elementSize=stride;d.label=label;
  d.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  d.defaultState=rhi::ResourceState::ShaderResource;
  return SLANG_SUCCEEDED(device->createBuffer(d,nullptr,out.writeRef()));
}
}
bool src_create(SplitRadianceCascades& s,rhi::IDevice* device,const SrcConfig& config,std::string& error) {
  if(!device||!src_validate(config,s.layout,error))return false;
  s.config=config;s.initialized=false;s.active=false;s.reset_pending=true;
  auto& l=s.layout;
  if(!buffer(device,s.probes,l.probes,64,"src_probes")||!buffer(device,s.hash,l.hash_entries,4,"src_hash")||
     !buffer(device,s.freelist,l.probes,4,"src_freelist")||!buffer(device,s.counters,64,4,"src_counters")||
     !buffer(device,s.fluence,l.directions,16,"src_interval_fp32")||!buffer(device,s.weights,l.directions,4,"src_weights_fp32")||
     !buffer(device,s.radiance,l.directions,16,"src_merged_fp32")||!buffer(device,s.previous,l.directions,16,"src_previous_fp32")||
     !buffer(device,s.rays,config.max_surface_rays,80,"src_surface_rays")||!buffer(device,s.links,l.probes,64,"src_visible_parent_links")||
     !buffer(device,s.oct,std::uint64_t(l.probes)*36,16,"src_probe_irradiance_oct")||
     !buffer(device,s.oct_prev,std::uint64_t(l.probes)*36,16,"src_probe_irradiance_oct_prev")||
     !buffer(device,s.distribution,l.probes,16,"src_ray_distribution")) {error="SRC buffer allocation failed";return false;}
  struct Program {const char* file;const char* entry;};
  const Program programs[]={
    {"Cache.slang","reset"},{"Cache.slang","maintain"},{"Cache.slang","decay"},{"Cache.slang","rehash"},
    {"Cache.slang","visible"},{"Cache.slang","secondary"},{"Cache.slang","parents"},{"Cache.slang","refresh"},
    {"RayDistribution.slang","count"},{"RayDistribution.slang","propagate"},{"RayDistribution.slang","offsets"},
    {"Trace.slang","primary"},{"Trace.slang","secondary"},{"Deposit.slang","main"},
    {"Links.slang","main"},{"Merge.slang","main"},{"Irradiance.slang","main"},{"Resolve.slang","main"}};
  static_assert(std::size(programs)==unsigned(SrcPass::Count));
  for(unsigned i=0;i<std::size(programs);++i) {
    std::string path="octaryn-client/Shaders/SplitRadianceCascades/";path+=programs[i].file;
    if(!create_rhi_compute_pipeline(device,path.c_str(),programs[i].entry,s.pipelines[i])) {
      error="SRC shader pipeline failed: "+path+" / "+programs[i].entry;return false;
    }
  }
  s.gpu_bytes=l.bytes;s.initialized=true;return true;
}
bool src_resize(SplitRadianceCascades& s,rhi::IDevice* device,unsigned width,unsigned height) {
  if(!width||!height||std::uint64_t(width)*height>16777216)return false;
  if(s.width==width&&s.height==height&&s.output)return true;
  const auto bytes=s.layout.bytes+std::uint64_t(width)*height*16;
  if(bytes>s.config.memory_limit)return false;
  rhi::TextureDesc d{};d.size={width,height,1};d.format=rhi::Format::RGBA32Float;d.label="src_irradiance_fp32";
  d.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  d.defaultState=rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;
  if(SLANG_FAILED(device->createTexture(d,nullptr,texture.writeRef()))||SLANG_FAILED(texture->getDefaultView(view.writeRef())))return false;
  s.output=texture;s.output_view=view;s.width=width;s.height=height;s.gpu_bytes=bytes;s.active=false;return true;
}
bool src_prepare(SplitRadianceCascades& s,rhi::ICommandEncoder* commands,unsigned epoch) {
  if(!s.initialized||!commands)return false;s.epoch=epoch;
  if(s.reset_pending) {
    for(auto* b:{s.probes.get(),s.hash.get(),s.freelist.get(),s.counters.get(),s.fluence.get(),s.weights.get(),
                 s.radiance.get(),s.previous.get(),s.rays.get(),s.links.get(),s.distribution.get()})commands->clearBuffer(b);
    s.reset_pending=false;
  }
  return true;
}
bool src_bind(const SplitRadianceCascades& s,rhi::IShaderObject* root,const char* name) {
  if(!root||!s.output_view)return false;auto field=rhi::ShaderCursor(root)[name];
  return !field.isValid()||SLANG_SUCCEEDED(field.setBinding(s.output_view));
}
}
