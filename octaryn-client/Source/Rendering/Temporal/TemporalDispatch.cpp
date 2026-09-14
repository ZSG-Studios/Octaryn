#include "WorldTemporal.h"
#include <algorithm>
#include <cstdio>

namespace octaryn::client::rendering {
WorldCamera begin_temporal(WorldTemporal& t,const WorldCamera& camera,std::uint64_t frame) {
  t.camera=camera;if(!t.mode)return camera;
  t.now=WorldTemporal::Clock::now();
  t.delta_ms=t.last.time_since_epoch().count()?std::chrono::duration<float,std::milli>(t.now-t.last).count():16.6667f;
  t.reset=t.history.reset(camera,int(t.width),int(t.height),t.delta_ms*.001,t.resolution.active);
  t.jitter=fsr2_jitter(static_cast<std::uint32_t>(frame),t.width,t.display_width);
  auto result=camera;result.jitter_x=-2*t.jitter.x/float(t.width);result.jitter_y=2*t.jitter.y/float(t.height);
  return result;
}
void commit_temporal(WorldTemporal& t) {
  if(!t.mode)return;
  if(t.reset)++t.reset_count;
  t.history.commit(t.camera,int(t.width),int(t.height));t.last=t.now;
}
bool prepare_temporal(WorldTemporal& t,rhi::ICommandEncoder* commands,unsigned slot,rhi::ITexture* depth,
    rhi::ITextureView* scene,rhi::ITextureView* material) {
  if(!t.mode)return true;
  auto& f=t.targets[slot];
  struct Uniforms {TemporalView current,previous;float dimensions[4],options[4];};
  const Uniforms uniforms{temporal_view(t.camera,int(t.width),int(t.height)),
      temporal_view(t.reset?t.camera:t.history.previous(),t.reset?int(t.width):t.history.previous_width(),
          t.reset?int(t.height):t.history.previous_height()),
      {float(t.width),float(t.height),-2*t.jitter.x/float(t.width),2*t.jitter.y/float(t.height)},
      {float(t.reset),0,0,0}};
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(t.inputs);
  bool ok=root && SLANG_SUCCEEDED(root->setData({0,0,0},&uniforms,sizeof(uniforms)));
  if(ok)ok=SLANG_SUCCEEDED(root->setBinding({0,0,0},rhi::Binding(depth)));
  const std::array<rhi::ITextureView*,6> bindings{f.object_view,f.opaque_view,scene,material,f.motion_view,f.reactive_view};
  for(unsigned i=0;ok && i<bindings.size();++i)ok=SLANG_SUCCEEDED(root->setBinding({0,i+1,0},rhi::Binding(bindings[i])));
  if(ok)pass->dispatchCompute((t.width+7)/8,(t.height+7)/8,1);
  pass->end();return ok;
}
bool resolve_temporal(WorldTemporal& t,rhi::ICommandEncoder* commands,unsigned slot,rhi::ITexture* depth,rhi::ITexture* scene) {
  if(!t.mode)return true;
  auto& f=t.targets[slot];Fsr2Dispatch desc{};
  desc.color=scene;desc.depth=depth;desc.motion_vectors=f.motion;desc.reactive=f.reactive;desc.transparency=f.reactive;
  desc.output=f.output;desc.render_width=t.width;desc.render_height=t.height;
  desc.jitter_x=t.jitter.x;desc.jitter_y=t.jitter.y;desc.motion_scale_x=float(t.width);desc.motion_scale_y=float(t.height);
  desc.sharpen=t.sharpening;desc.sharpness=t.sharpness;
  desc.delta_ms=std::clamp(t.delta_ms,1.f,1000.f);desc.vertical_fov=t.camera.vertical_fov;desc.reset=t.reset;
  if(dispatch_fsr2(t.fsr,commands,desc))return true;
  std::fprintf(stderr,"FSR2 dispatch failed: %s\n",fsr2_error(t.fsr));return false;
}
}
