#include "BlockTransportInternal.h"
#include "BlockTransportLighting.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <array>
#include <cstdio>

namespace octaryn::client::rendering {
namespace {
template<class T> bool data(rhi::ShaderCursor c,const char* name,const T& value) {
  auto field=c[name];return !field.isValid() || world_rhi_ok(field.setData(&value,sizeof(value)));
}
bool buffer(rhi::ShaderCursor c,const char* name,rhi::IBuffer* value) {
  auto field=c[name];return !field.isValid() || (value && world_rhi_ok(field.setBinding(rhi::Binding(value))));
}
std::uint32_t next_epoch(std::uint32_t current) {return current>=UINT32_MAX-2?1:current+1;}
bool bindings(WorldRenderer& r,rhi::IShaderObject* root,unsigned order) {
  auto& s=r.block_gi;rhi::ShaderCursor c(root);
  const std::array<unsigned,4> frame{s.pending_epoch,unsigned(r.frames),BlockTransportCapacity,BlockTransportLinks};
  const std::array<unsigned,4> work{0,BlockTransportRows,s.pending_radiance_epoch,0};
  const std::array<unsigned,4> solve{s.pending_radiance_epoch,BlockTransportCapacity,order,BlockTransportLinks};
  auto* output=order<3?s.orders[order].get():s.output.get();
  // Uniform branches still expose all SRVs. Never bind the output as an unused SRV.
  const std::array<rhi::IBuffer*,3> inputs{
      order==0?s.orders[1].get():s.orders[0].get(),
      order==1?s.orders[2].get():s.orders[1].get(),
      order==2?s.orders[0].get():s.orders[2].get()};
  return data(c,"btFrameInfo",frame) && data(c,"btWork",work) && data(c,"btSolve",solve) &&
      data(c,"btPlayerRevision",s.pending_player_revision) &&
      data(c,"btContributorMin",s.admission.contributor_minimum) &&
      data(c,"btContributorMax",s.admission.contributor_maximum) &&
      data(c,"btContributorLimit",s.stats.contributor_limit) &&
      data(c,"btSun",s.pending_sun) && data(c,"btSky",s.pending_sky) &&
      data(c,"btCoverageMin",s.coverage_min) && data(c,"btCoverageMax",s.coverage_max) &&
      data(c,"giLightNodeCount",s.light_nodes) &&
      buffer(c,"btSurfaces",s.surfaces) && buffer(c,"btLinks",s.links) &&
      buffer(c,"btCandidates",s.candidates) && buffer(c,"btCounters",s.counters) &&
      buffer(c,"btWorkRows",s.work_rows) && buffer(c,"btWorkBlocks",s.work_blocks) &&
      buffer(c,"btSchedule",s.schedule) && buffer(c,"btIndirect",s.output) &&
      buffer(c,"btDirect",s.direct_values) && buffer(c,"btEnvironment",s.environment) &&
      buffer(c,"btB0",inputs[0]) && buffer(c,"btB1",inputs[1]) && buffer(c,"btB2",inputs[2]) &&
      buffer(c,"btOutput",output) &&
      buffer(c,"giLightTree",s.light_tree) && buffer(c,"localLights",r.local_lighting.light_buffer);
}
bool dispatch(WorldRenderer& r,rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,
    unsigned x,unsigned y=1,unsigned order=3,bool rays=false) {
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(pipeline);
  bool ok=root && bindings(r,root,order);
  if(ok)ok=bind_world_atlas(r.atlas,root);
  if(ok && rays)ok=world_ray_bind(r,root);
  if(ok)pass->dispatchCompute(x,y,1);
  pass->end();
  if(ok)commands->globalBarrier();
  return ok;
}
}
bool render_block_transport_gi(WorldRenderer& r,rhi::ICommandEncoder* commands,bool geometry_ready) {
  auto& s=r.block_gi;s.active=false;s.pending=false;s.pending_eviction=false;s.inputs_pending=false;
  s.stats.coverage_valid=false;s.stats.transport_ready=false;s.stats.scheduled_rows=0;
  s.stats.admission_faces=0;s.stats.admission_columns=0;
  if(!commands)return false;
  if(!resolve_block_transport_statistics(r))return false;
  if(!prepare_block_transport_gi(r) || !prepare_block_transport_lights(r,commands))return false;
  s.inputs_pending=true;
  if(!geometry_ready || !r.ray_enabled || !world_ray_available(r) || !world_ray_coverage_complete(r) ||
      !update_block_transport_coverage(r)) {s.valid=false;s.stats.transport_ready=false;return true;}
  s.pending_scene_revision=r.scene_changes.revision();
  s.admission.pending_generation=world_ray_stats(r).scene_generation;
  const std::array<float,4> origin{r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
  float moved_squared=0;
  for(unsigned axis=0;axis<3;++axis) {
    const float delta=origin[axis]-s.cache_origin[axis];moved_squared+=delta*delta;
  }
  const bool recenter=s.initialized && moved_squared>=16.f*16.f;
  const bool reset=!s.initialized || !s.valid || recenter || s.scene_revision!=s.pending_scene_revision ||
      s.admission.generation!=s.admission.pending_generation;
  s.pending_cache_origin=reset?origin:s.cache_origin;
  if(!prepare_block_transport_admission(r,reset)) {s.valid=false;s.stats.transport_ready=false;return true;}
  if(reset)s.stats.transport_ready=false;
  s.pending_epoch=reset?next_epoch(s.epoch):s.epoch;
  s.stats.contributor_limit=block_contributor_limit(!reset && s.stats.mandatory_ready &&
      s.stats.measured_epoch==s.epoch,s.stats.pinned_rows);
  s.pending_eviction=!reset && s.stats.measured_epoch==s.epoch && s.stats.counters[11]!=s.pressure_count;
  s.pending_pressure_count=reset?0:s.pending_eviction?s.stats.counters[11]:s.pressure_count;
  s.pending_lighting_config={r.lighting_config.ambient_strength,r.lighting_config.sun_strength,
      r.lighting_config.sun_fallback_strength,r.lighting_config.skylight_floor};
  const auto lighting=block_transport_lighting(
      {-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength},
      {r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0});
  s.pending_sun=lighting.sun;s.pending_sky=lighting.sky;
  const bool relight=reset || s.tree_revision!=s.pending_tree_revision || s.lighting_config!=s.pending_lighting_config ||
      block_transport_lighting_discontinuity({s.sun,s.sky},lighting);
  s.pending_radiance_epoch=relight?next_epoch(s.radiance_epoch):s.radiance_epoch;
  s.pending_player_signature=player_occlusion_signature(r.player,r.player_pose);
  s.pending_player_revision=s.pending_player_signature==s.player_signature?s.player_revision:
      s.player_revision>=0x3fffffffu?1:s.player_revision+1;
  if(relight)s.stats.transport_ready=false;
  r.lighting_profile.begin_pass(commands,LightingPass::DiffuseTrace);
  if(reset) {
    if(!dispatch(r,commands,s.clear,(BlockTransportCapacity+63)/64))return false;
    std::printf("block_transport_reset frame=%llu geometry_epoch=%u radiance_epoch=%u scene_revision=%llu recenter=%u\n",
        static_cast<unsigned long long>(r.frames),s.pending_epoch,s.pending_radiance_epoch,
        static_cast<unsigned long long>(s.pending_scene_revision),recenter?1u:0u);
  }
  if(s.pending_eviction && !dispatch(r,commands,s.evict,(BlockTransportCapacity+63)/64))return false;
  const unsigned candidate_groups=(BlockTransportRows*BlockTransportLinks+63)/64;
  const unsigned selection_groups=(BlockTransportCapacity+BlockTransportSelectGroup-1)/BlockTransportSelectGroup;
  if(!admit_block_transport_world(r,commands) ||
      !dispatch(r,commands,s.select_count,selection_groups) ||
      !dispatch(r,commands,s.select_prefix,1) ||
      !dispatch(r,commands,s.select_rows,selection_groups) ||
      !dispatch(r,commands,s.trace,candidate_groups,1,3,true) ||
      !dispatch(r,commands,s.admit_contributors,candidate_groups) ||
      !dispatch(r,commands,s.resolve_contributors,candidate_groups) ||
      !dispatch(r,commands,s.direct,(BlockTransportRows+63)/64,1,3,true))return false;
  r.lighting_profile.mark(commands,LightingPass::DiffuseTrace);
  r.lighting_profile.begin_pass(commands,LightingPass::DiffuseFilter);
  for(unsigned order=0;order<4;++order) {
    if(!dispatch(r,commands,s.bounce,(BlockTransportCapacity+63)/64,1,order))return false;
    commands->setBufferState(order<3?s.orders[order].get():s.output.get(),rhi::ResourceState::ShaderResource);
  }
  r.lighting_profile.mark(commands,LightingPass::DiffuseFilter);
  commands->copyBuffer(s.readback[r.active_frame],0,s.counters,0,sizeof(s.stats.counters));
  commands->copyBuffer(s.readback[r.active_frame],sizeof(s.stats.counters),s.schedule,0,64);
  commands->setBufferState(s.counters,rhi::ResourceState::UnorderedAccess);
  commands->setBufferState(s.schedule,rhi::ResourceState::UnorderedAccess);
  s.active=true;s.pending=true;
  s.stats.epoch=s.pending_epoch;
  return true;
}
void commit_block_transport_gi(WorldRenderer& r) {
  auto& s=r.block_gi;
  if(s.inputs_pending) {s.tree_revision=s.pending_tree_revision;s.inputs_pending=false;}
  if(!s.pending)return;
  const bool reset=s.epoch!=s.pending_epoch;
  if(reset) {++s.stats.resets;s.stats.frames_since_reset=0;}
  commit_block_transport_admission(r,reset);
  if(s.stats.contributor_limit>0 && s.stats.contributor_start_frame==UINT64_MAX)
    s.stats.contributor_start_frame=r.frames;
  if(s.radiance_epoch!=s.pending_radiance_epoch)s.stats.transport_ready=false;
  ++s.stats.frames_since_reset;
  s.initialized=true;s.valid=true;s.pending=false;
  s.epoch=s.pending_epoch;s.radiance_epoch=s.pending_radiance_epoch;
  s.scene_revision=s.pending_scene_revision;s.tree_revision=s.pending_tree_revision;
  s.player_signature=s.pending_player_signature;s.player_revision=s.pending_player_revision;
  s.sun=s.pending_sun;s.sky=s.pending_sky;s.lighting_config=s.pending_lighting_config;
  s.cache_origin=s.pending_cache_origin;s.pressure_count=s.pending_pressure_count;
  if(s.pending_eviction)++s.stats.eviction_passes;
  s.readback_ready[r.active_frame]=true;s.readback_frame[r.active_frame]=r.frames;
  s.readback_sweeps[r.active_frame]=s.stats.admission_sweeps;
  s.readback_epoch[r.active_frame]=s.epoch;s.readback_radiance_epoch[r.active_frame]=s.radiance_epoch;
  ++s.stats.submitted_frames;
}
}
