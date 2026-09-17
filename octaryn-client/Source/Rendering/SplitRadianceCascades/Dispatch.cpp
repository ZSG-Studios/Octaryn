#include "System.h"
#include "../RenderBackend/LightingProfile.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace octaryn::client::rendering {
namespace {
template<class T> bool data(rhi::ShaderCursor root,const char* name,const T& value) {
  auto field=root[name];return !field.isValid()||SLANG_SUCCEEDED(field.setData(&value,sizeof(value)));
}
bool bind(rhi::ShaderCursor root,const char* name,rhi::IBuffer* buffer) {
  auto field=root[name];return !field.isValid()||SLANG_SUCCEEDED(field.setBinding(rhi::Binding(buffer)));
}
bool texture(rhi::ShaderCursor root,const char* name,rhi::ITextureView* value) {
  auto field=root[name];return !field.isValid()||SLANG_SUCCEEDED(field.setBinding(value));
}
void barrier(SplitRadianceCascades&,rhi::ICommandEncoder*) {
  // Standalone RHI tracks UAV/SRV transitions from shader bindings; explicit
  // global barriers per pass made debug-layer command validation cost hundreds
  // of milliseconds per frame.
}
bool run(SplitRadianceCascades& s,rhi::ICommandEncoder* commands,const SrcFrame& frame,SrcPass kind,unsigned count,unsigned level=0,unsigned stage=0) {
  barrier(s,commands);auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(s.pipelines[unsigned(kind)]);bool ok=root!=nullptr;
  if(ok) {
    rhi::ShaderCursor c(root);const auto& cfg=s.config;
    const unsigned extent[]={frame.width,frame.height,frame.frame,frame.geometry_epoch};
    const unsigned counts[]={cfg.cascades,s.layout.probes,s.layout.directions,s.layout.hash_entries};
    const unsigned limits[]={cfg.max_surface_rays,cfg.hash_search_limit,cfg.visible_lifetime,cfg.secondary_lifetime};
    const float intervals[]={cfg.spacing,cfg.contact_length,cfg.base_interval,cfg.interval_growth};
    const float temporal[]={cfg.decay,cfg.feedback,cfg.max_trace_distance,0};
    const float lod[]={cfg.lod_radius,cfg.lod_blend,0,0};
    ok=data(c,"srcCascades",s.layout.cascades)&&data(c,"srcExtent",extent)&&data(c,"srcCounts",counts)&&
       data(c,"srcLimits",limits)&&data(c,"srcIntervals",intervals)&&data(c,"srcTemporal",temporal)&&
       data(c,"srcLod",lod)&&data(c,"srcEye",frame.eye)&&data(c,"srcOrigin",frame.origin)&&data(c,"srcLevel",level)&&data(c,"srcStage",stage)&&
       bind(c,"srcProbes",s.probes)&&bind(c,"srcHash",s.hash)&&bind(c,"srcFreelist",s.freelist)&&
       bind(c,"srcCounters",s.counters)&&bind(c,"srcFluence",s.fluence)&&bind(c,"srcWeights",s.weights)&&
       bind(c,"srcRadiance",s.radiance)&&bind(c,"srcPrevious",s.previous)&&bind(c,"srcRays",s.rays)&&bind(c,"srcLinks",s.links)&&bind(c,"srcDistribution",s.distribution)&&bind(c,"srcOct",s.oct)&&bind(c,"srcOctPrev",s.oct_prev)&&
       texture(c,"srcPositions",frame.positions)&&texture(c,"srcNormals",frame.normals)&&texture(c,"srcLighting",s.output_view);
    if(ok&&(kind==SrcPass::Trace||kind==SrcPass::TraceSecondary||kind==SrcPass::Resolve||kind==SrcPass::Links))
      ok=frame.bind_trace&&frame.bind_trace(root,frame.trace_context);
  }
  if(ok) {
    if(kind==SrcPass::Resolve)pass->dispatchCompute((frame.width+7)/8,(frame.height+7)/8,1);
    else {const auto groups=(std::uint64_t(count)+63)/64;pass->dispatchCompute(unsigned(std::min<std::uint64_t>(groups,65535)),unsigned((groups+65534)/65535),1);}
  }
  pass->end();
  if(!ok)std::fprintf(stderr,"src_pass_failed pass=%u level=%u stage=%u count=%u\n",
      unsigned(kind),level,stage,count);
  return ok;
}
}
bool src_dispatch(SplitRadianceCascades& s,rhi::ICommandEncoder* commands,const SrcFrame& f) {
  s.active=false;if(!s.initialized||!s.output||!f.positions||!f.normals||!commands)return false;
  const auto primary=std::min(std::uint64_t(f.width)*f.height,std::uint64_t(s.config.max_surface_rays/2));
  auto pass=[&](SrcPass p,unsigned count,unsigned level=0,unsigned stage=0){return run(s,commands,f,p,count,level,stage);};
  auto parents=[&](){for(unsigned c=0;c+1<s.config.cascades;++c)if(!pass(SrcPass::Parents,s.layout.cascades[c].capacity,c))return false;return true;};
  auto begin=[&](LightingPass p){if(f.profile)f.profile->begin_pass(commands,p);};
  auto end=[&](LightingPass p){if(f.profile)f.profile->mark(commands,p);};
  begin(LightingPass::SrcSeed);
  if(!pass(SrcPass::Reset,std::max(s.layout.hash_entries,64u))||!pass(SrcPass::Maintain,s.layout.probes)||
     !pass(SrcPass::Decay,s.layout.directions))return false;
  // Snapshot after invalidation and before any new probe can reuse a slot.
  commands->copyBuffer(s.previous,0,s.radiance,0,s.layout.directions*std::uint64_t(16));
  commands->copyBuffer(s.oct_prev,0,s.oct,0,std::uint64_t(s.layout.probes)*36*16);
  commands->setBufferState(s.previous,rhi::ResourceState::ShaderResource);
  if(!pass(SrcPass::Rehash,s.layout.probes)||!pass(SrcPass::Visible,f.width*f.height)||!parents()||
     !pass(SrcPass::Refresh,f.width*f.height))return false;
    if(!pass(SrcPass::RayCount,unsigned(primary)))return false;
  for(unsigned c=0;c+1<s.config.cascades;++c)
    if(!pass(SrcPass::RayPropagate,s.layout.cascades[c].capacity,c))return false;
  for(unsigned c=s.config.cascades;c-->0;)
    if(!pass(SrcPass::RayOffsets,s.layout.cascades[c].capacity,c))return false;
  end(LightingPass::SrcSeed);  begin(LightingPass::SrcTrace);
  if(!pass(SrcPass::Trace,unsigned(primary))||
     !pass(SrcPass::Secondary,unsigned(primary))||!parents()||!pass(SrcPass::Refresh,unsigned(primary),0,1)||
     !pass(SrcPass::TraceSecondary,unsigned(primary)))return false;
  end(LightingPass::SrcTrace);  begin(LightingPass::SrcDeposit);
  if(!pass(SrcPass::Deposit,unsigned(primary*2)))return false;
  end(LightingPass::SrcDeposit);  begin(LightingPass::SrcMerge);
  if(!pass(SrcPass::Links,s.layout.probes))return false;
  for(unsigned c=s.config.cascades;c-->0;) {
    const auto& level=s.layout.cascades[c];
    if(!pass(SrcPass::Merge,level.capacity*level.directions,c))return false;
  }
  end(LightingPass::SrcMerge);
  begin(LightingPass::SrcEvaluate);
  if(!pass(SrcPass::Evaluate,s.layout.probes))return false;
  end(LightingPass::SrcEvaluate);
  begin(LightingPass::SrcContact);
  commands->setTextureState(s.output,rhi::ResourceState::UnorderedAccess);
  if(!pass(SrcPass::Resolve,0))return false;
  end(LightingPass::SrcContact);
  commands->setTextureState(s.output,rhi::ResourceState::ShaderResource);s.active=true;return true;
}
}
