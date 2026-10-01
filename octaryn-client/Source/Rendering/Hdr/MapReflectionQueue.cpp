#include "MapReflectionQueue.h"
#include "MapReflectionScreen.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <algorithm>
namespace octaryn::client::rendering {
namespace {
bool buffer(rhi::IDevice* device,Slang::ComPtr<rhi::IBuffer>& output,std::uint64_t size,unsigned stride,bool indirect=false) {
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  if(indirect)desc.usage|=rhi::BufferUsage::IndirectArgument;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  return world_rhi_ok(device->createBuffer(desc,nullptr,output.writeRef()));
}
// Environment options are fixed for the process; read once like the screen
// variant flags in MapReflectionScreen.h instead of per frame.
struct QueueEnvOptions { bool queued,recovery,coherent; };
const QueueEnvOptions& queue_env_options() {
  static const QueueEnvOptions options=[] {
    const auto flag=[](const char* name) {
      const auto* value=std::getenv(name);
      return value && value[0]=='1' && value[1]=='\0';
    };
    return QueueEnvOptions{flag("OCTARYN_CLIENT_RT_QUEUED"),
        flag("OCTARYN_CLIENT_RT_QUEUE_REFERENCE_RECOVERY"),
        flag("OCTARYN_CLIENT_RT_QUEUE_COHERENT_RECOVERY")};
  }();
  return options;
}
bool bindings(WorldRenderer& r,rhi::IShaderObject* root,bool valid,const float* dimensions) {
  if(!root || !world_ray_bind(r,root))return false;
  auto& s=r.map_reflections;auto& q=s.queue;auto& hdr=r.target().hdr;
  auto& previous=s.history[s.index];auto& current=s.history[1-s.index];
  rhi::ShaderCursor c(root);
  const auto bind=[&](const char* name,rhi::Binding resource) {
    auto field=c[name];return !field.isValid() || world_rhi_ok(field.setBinding(resource));
  };
  const auto data=[&](const char* name,const void* value,std::size_t size=16) {
    auto field=c[name];return !field.isValid() || world_rhi_ok(field.setData(value,size));
  };
  auto camera=r.temporal.camera;
  if(r.temporal.mode) {
    camera.jitter_x=2*r.temporal.jitter.x/float(r.render_width());
    camera.jitter_y=-2*r.temporal.jitter.y/float(r.render_height());
  }
  const auto& old=valid?s.camera.previous():camera;
  const auto prior=temporal_view(old,r.render_width(),r.render_height());
  const auto view=temporal_view(camera,r.render_width(),r.render_height());
  const float eye[4]={camera.x,camera.y,camera.z,0};
  const float jitter[4]={old.jitter_x,old.jitter_y,0,0};
  const float current_jitter[4]={camera.jitter_x,camera.jitter_y,0,0};
  const float source[4]={float(r.render_width()),float(r.render_height()),0,0};
  const float sun[4]={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
  const float lighting[4]={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0};
  const unsigned reference=0,search=s.history_search?1u:0u;
  const unsigned reference_recovery=q.reference_recovery?1u:0u;
  return bind("reflectionReceivers",q.receivers) && bind("reflectionRecovery",q.recovery) &&
      bind("reflectionCounts",q.counts) && bind("reflectionIntersections",q.intersections) &&
      bind("recoverySamples",q.recovery_samples) && bind("recoveryHigh",q.recovery_high) && bind("recoveryLow",q.recovery_low) &&
      bind("colors",hdr.views[0]) && bind("positions",hdr.views[1]) && bind("voxels",hdr.views[2]) && bind("materials",hdr.views[3]) &&
      bind("emissive",hdr.views[4]) && bind("surfaceFlags",hdr.views[5]) &&
      bind("previousRadiance",previous.radiance.view) && bind("previousPosition",previous.position.view) &&
      bind("previousSurface",previous.surface.view) && bind("previousMaterial",previous.material.view) &&
      bind("previousMoments",previous.moments.view) && bind("reflectionHistory",current.radiance.view) &&
      bind("reflectionPosition",current.position.view) && bind("reflectionSurface",current.surface.view) &&
      bind("reflectionMaterial",current.material.view) && bind("reflectionMoments",current.moments.view) &&
      data("referenceMode",&reference,4) && data("historySearch",&search,4) && data("eye",eye) &&
      data("referenceRecovery",&reference_recovery,4) && data("recoveryRayBudget",&q.recovery_ray_budget,4) &&
      data("dimensions",dimensions) && data("sourceDimensions",source) && data("sun",sun) && data("lighting",lighting) &&
      data("previousPositionCamera",prior.position.data()) && data("previousRight",prior.right.data()) &&
      data("previousUp",prior.up.data()) && data("previousForward",prior.forward.data()) &&
      data("previousProjection",prior.projection.data()) && data("previousJitter",jitter) && data("currentForward",view.forward.data()) &&
      data("currentRight",view.right.data()) && data("currentUp",view.up.data()) &&
      data("currentProjection",view.projection.data()) && data("currentJitter",current_jitter);
}
}
bool prepare_map_reflection_queue(WorldRenderer& r,unsigned width,unsigned height) {
  auto& q=r.map_reflections.queue;
  const auto& env=queue_env_options();
  // Sticky fallback: once the queue failed it must not be retried every frame.
  q.enabled=env.queued && !r.map_reflections.reference && !r.map_reflections.queue_disabled;
  q.reference_recovery=env.recovery;
  q.coherent_recovery=!q.reference_recovery && env.coherent;
  q.screen_enabled=q.enabled && map_reflection_screen_supported(r.device);
  if(!q.enabled)return true;
  if(r.block_transport_lookup.active) {r.status="queued_reflections_require_direct_map_lighting";return false;}
  const std::uint64_t pixels=std::uint64_t(width)*height;
  if(!pixels || pixels>0xffffffu) {r.status="queued_reflection_extent_out_of_range";return false;}
  if(!q.pipelines[0]) {
    constexpr const char* entries[]={"clear_main","classify_main","arguments_main","intersect_main","shade_main","recovery_main",
        "recovery_classify_main","recovery_refine_main"};
    for(unsigned i=0;i<q.pipelines.size();++i) {
      const char* path=i==0 || i==2?"octaryn-client/Shaders/Hdr/MapReflectionQueueArguments.slang":
          i>=6?"octaryn-client/Shaders/Hdr/MapReflectionRecovery.slang":"octaryn-client/Shaders/Hdr/MapReflectionQueue.slang";
      if(q.coherent_recovery && i!=0 && i!=2)path=i>=6?
          "octaryn-client/Shaders/Hdr/MapReflectionRecoveryCoherent.slang":
          "octaryn-client/Shaders/Hdr/MapReflectionQueueCoherent.slang";
      if(q.screen_enabled && i!=0 && i!=2 && i<6)path=q.coherent_recovery?
          "octaryn-client/Shaders/Hdr/MapReflectionQueueScreenCoherent.slang":
          "octaryn-client/Shaders/Hdr/MapReflectionQueueScreen.slang";
      if(!create_rhi_compute_pipeline(r.device,path,entries[i],q.pipelines[i]))return false;
    }
    if(q.screen_enabled) {
      const char* path=q.coherent_recovery?"octaryn-client/Shaders/Hdr/MapReflectionQueueScreenCoherent.slang":
          "octaryn-client/Shaders/Hdr/MapReflectionQueueScreen.slang";
      if(!create_rhi_compute_pipeline(r.device,path,"screen_main",q.screen) || !prepare_map_reflection_coverage(r))return false;
      std::printf("map_reflections screen_hits=1 conservative_coverage=1 maximum_screen_cells=96 candidate=1 opaque_occlusion_qualified=0\n");
    } else if(map_reflection_screen_requested()) {
      std::printf("map_reflections screen_hits=0 reason=virtual_geometry_requires_ray_scene_intersections\n");
    }
  }
  if(q.capacity>=pixels)return true;
  if(!r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms()))return false;
  if(!buffer(r.device,q.receivers,pixels*4,4) || !buffer(r.device,q.recovery,pixels*4,4) ||
      !buffer(r.device,q.intersections,pixels*64,32) || !buffer(r.device,q.counts,32,4) ||
      !buffer(r.device,q.recovery_samples,pixels*32,32) || !buffer(r.device,q.recovery_high,pixels*4,4) ||
      !buffer(r.device,q.recovery_low,pixels*4,4) || !buffer(r.device,q.arguments,36,4,true))return false;
  q.capacity=unsigned(pixels);q.bytes=pixels*112+68;
  std::printf("map_reflection_queue enabled=1 capacity=%u bytes=%llu recovery=%s coherent=%u extra_queries_per_pixel_ceiling=0.5 candidate=1\n",
      q.capacity,static_cast<unsigned long long>(q.bytes),q.reference_recovery?"reference_directions":"variance_budgeted",q.coherent_recovery?1u:0u);
  return true;
}
bool render_map_reflection_queue(WorldRenderer& r,rhi::ICommandEncoder* commands,bool valid,const float* dimensions) {
  auto& s=r.map_reflections;auto& q=s.queue;
  const auto& resolution=r.temporal.resolution;
  if(resolution.active && resolution.samples>=12 && resolution.samples!=q.feedback_samples && resolution.average_ms>0) {
    const float ratio=resolution.budget_ms()/resolution.average_ms;
    if(ratio<.96f)q.recovery_budget_scale=std::max(.125f,q.recovery_budget_scale*ratio);
    else if(ratio>1.08f)q.recovery_budget_scale=std::min(1.f,q.recovery_budget_scale+.01f);
  }
  if(!resolution.active || resolution.samples<q.feedback_samples)q.recovery_budget_scale=1;
  q.feedback_samples=resolution.samples;
  q.recovery_ray_budget=unsigned(double(s.width)*s.height*.5*q.recovery_budget_scale);
  const auto dispatch=[&](unsigned index,bool indirect,unsigned offset=0) {
    auto* pass=commands->beginComputePass();if(!pass)return false;
    auto* root=pass->bindPipeline(index==8?q.screen:q.pipelines[index]);bool ok=root!=nullptr;
    if(ok && (index==0 || index==2)) {
      rhi::ShaderCursor c(root);
      ok=world_rhi_ok(c["reflectionCounts"].setBinding(rhi::Binding(q.counts))) &&
          world_rhi_ok(c["reflectionArguments"].setBinding(rhi::Binding(q.arguments)));
      auto budget=c["recoveryRayBudget"];
      if(ok && budget.isValid())ok=world_rhi_ok(budget.setData(&q.recovery_ray_budget,sizeof(q.recovery_ray_budget)));
    } else if(ok)ok=bindings(r,root,valid,dimensions);
    if(ok) {
      if(indirect)pass->dispatchComputeIndirect({q.arguments,offset});
      else if(index==1)pass->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
      else pass->dispatchCompute(1,1,1);
    }
    pass->end();commands->globalBarrier();return ok;
  };
  r.lighting_profile.begin_pass(commands,LightingPass::ReflectionClassify);
  commands->setBufferState(q.counts,rhi::ResourceState::UnorderedAccess);
  commands->setBufferState(q.arguments,rhi::ResourceState::UnorderedAccess);
  if(!dispatch(0,false) || !dispatch(1,false) || !dispatch(2,false))return false;
  commands->setBufferState(q.arguments,rhi::ResourceState::IndirectArgument);
  r.lighting_profile.mark(commands,LightingPass::ReflectionClassify);
  if(q.screen_enabled) {
    r.lighting_profile.begin_pass(commands,LightingPass::ReflectionScreen);
    if(!dispatch(8,true,0))return false;
    r.lighting_profile.mark(commands,LightingPass::ReflectionScreen);
  }
  r.lighting_profile.begin_pass(commands,LightingPass::ReflectionIntersect);
  if(!dispatch(3,true,0))return false;
  r.lighting_profile.mark(commands,LightingPass::ReflectionIntersect);
  r.lighting_profile.begin_pass(commands,LightingPass::ReflectionShade);
  if(!dispatch(4,true,0))return false;
  r.lighting_profile.mark(commands,LightingPass::ReflectionShade);
  r.lighting_profile.begin_pass(commands,q.reference_recovery?LightingPass::ReflectionRecovery:LightingPass::ReflectionRecoveryBase);
  if(!dispatch(5,true,12))return false;
  r.lighting_profile.mark(commands,q.reference_recovery?LightingPass::ReflectionRecovery:LightingPass::ReflectionRecoveryBase);
  if(!q.reference_recovery) {
    r.lighting_profile.begin_pass(commands,LightingPass::ReflectionRecoveryClassify);
    if(!dispatch(6,true,12))return false;
    commands->setBufferState(q.arguments,rhi::ResourceState::UnorderedAccess);
    if(!dispatch(2,false))return false;
    commands->setBufferState(q.arguments,rhi::ResourceState::IndirectArgument);
    r.lighting_profile.mark(commands,LightingPass::ReflectionRecoveryClassify);
    r.lighting_profile.begin_pass(commands,LightingPass::ReflectionRecoveryRefine);
    if(!dispatch(7,true,24))return false;
    r.lighting_profile.mark(commands,LightingPass::ReflectionRecoveryRefine);
  }
  return true;
}
}
