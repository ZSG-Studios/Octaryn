#include "BlockTransportInternal.h"
#include "GILights.h"
#include "WorldRendererInternal.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace octaryn::client::rendering {
bool prepare_block_transport_lights(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.block_gi;s.pending_tree_revision=r.local_lighting.light_revision;
  if(s.tree_revision==s.pending_tree_revision)return true;
  const auto nodes=build_gi_light_tree(r.local_lighting.lights);
  const auto bytes=nodes.size()*sizeof(GILightNode);
  if(bytes>s.light_tree->getDesc().size) {r.status="block_transport_light_capacity";return false;}
  if(bytes && !world_rhi_ok(commands->uploadBufferData(s.light_tree,0,bytes,nodes.data())))return false;
  commands->setBufferState(s.light_tree,rhi::ResourceState::ShaderResource);
  s.light_nodes=static_cast<unsigned>(nodes.size());s.stats.light_nodes=s.light_nodes;
  return true;
}
bool update_block_transport_coverage(WorldRenderer& r) {
  auto& s=r.block_gi;s.coverage_min={};s.coverage_max={};s.stats.coverage_valid=false;
  if(r.sources.empty() || r.map)return false;
  const auto& first=r.sources.begin()->second;
  std::int64_t min_x=INT64_MAX,min_z=INT64_MAX,max_x=INT64_MIN,max_z=INT64_MIN;
  std::int64_t min_y=INT64_MAX,max_y=INT64_MIN;
  for(const auto& [coordinate,source]:r.sources) {
    if(source.height<=0 || source.height>512 || source.min_y!=first.min_y || source.height!=first.height ||
        source.blocks.size()!=std::size_t(source.height)*1024 ||
        source.x!=coordinate.first || source.z!=coordinate.second)return false;
    min_x=std::min(min_x,std::int64_t(source.x));max_x=std::max(max_x,std::int64_t(source.x));
    min_z=std::min(min_z,std::int64_t(source.z));max_z=std::max(max_z,std::int64_t(source.z));
    min_y=std::min(min_y,std::int64_t(source.min_y));
    max_y=std::max(max_y,std::int64_t(source.min_y)+source.height);
  }
  const auto width=max_x-min_x+1,depth=max_z-min_z+1;
  if(width>65 || depth>65 || width*depth!=static_cast<std::int64_t>(r.sources.size()))return false;
  s.coverage_min={float(min_x*32),float(min_y),float(min_z*32),1};
  s.coverage_max={float((max_x+1)*32),float(max_y),float((max_z+1)*32),0};
  s.stats.coverage_valid=true;return true;
}
bool resolve_block_transport_statistics(WorldRenderer& r) {
  auto& s=r.block_gi;const auto slot=r.active_frame;
  if(slot>=s.readback.size())return false;
  if(!s.readback_ready[slot])return true;
  // An explicit fenced capture can already have consumed a newer frame.
  if(s.readback_frame[slot]<s.stats.measured_frame) {s.readback_ready[slot]=false;return true;}
  void* mapped{};
  if(!world_rhi_ok(r.device->mapBuffer(s.readback[slot],rhi::CpuAccessMode::Read,&mapped)) || !mapped)return false;
  std::memcpy(s.stats.counters.data(),mapped,sizeof(s.stats.counters));
  std::array<std::uint32_t,16> schedule{};
  std::memcpy(schedule.data(),static_cast<const unsigned char*>(mapped)+sizeof(s.stats.counters),sizeof(schedule));
  if(!world_rhi_ok(r.device->unmapBuffer(s.readback[slot])))return false;
  s.stats.measured_frame=s.readback_frame[slot];s.stats.measured_epoch=s.readback_epoch[slot];
  s.stats.measured_radiance_epoch=s.readback_radiance_epoch[slot];
  s.stats.occupied_rows=s.stats.counters[8];s.stats.initialized_rows=s.stats.counters[9];
  s.stats.row_refreshes=s.stats.counters[10];s.readback_ready[slot]=false;
  s.stats.scheduled_rows=schedule[2];s.stats.selection_occupied=schedule[3];s.stats.ready_rows=schedule[4];
  s.stats.selection_start=schedule[1];s.stats.selection_wraps=schedule[8];
  s.stats.player_visibility_calls=schedule[9];s.stats.player_history_visibility_calls=schedule[10];
  s.stats.pinned_rows=schedule[11];s.stats.measured_sweeps=s.readback_sweeps[slot];
  if(s.stats.measured_epoch==s.epoch && schedule[5]==s.epoch &&
      schedule[7]==std::uint32_t(s.stats.measured_frame) && s.stats.pinned_rows<=s.stats.occupied_rows &&
      s.stats.occupied_rows<=BlockTransportCapacity)
    s.stats.mandatory_ready=block_mandatory_clean_sweep(s.stats.measured_sweeps,s.stats.counters[11],
        s.stats.checked_sweeps,s.stats.checked_admission_failures,s.stats.mandatory_ready);
  else s.stats.mandatory_ready=false;
  s.stats.transport_ready=s.valid && s.stats.mandatory_ready && s.stats.admission_sweeps>0 &&
      block_contributor_phase_ready(s.stats.measured_frame,s.stats.contributor_start_frame) && s.stats.measured_epoch==s.epoch &&
      s.stats.measured_radiance_epoch==s.radiance_epoch && schedule[5]==s.epoch && schedule[6]==s.radiance_epoch &&
      schedule[7]==std::uint32_t(s.stats.measured_frame) && s.stats.measured_frame>=s.stats.admission_complete_frame &&
      s.stats.occupied_rows>0 && s.stats.selection_occupied==s.stats.occupied_rows &&
      s.stats.ready_rows==s.stats.occupied_rows && s.stats.initialized_rows==s.stats.occupied_rows;
  if(r.frames%120==0) {
    const auto& c=s.stats.counters;
    std::printf("block_transport_statistics frame=%llu measured_frame=%llu measured_epoch=%u epoch=%u radiance_epoch=%u admission_attempts=%u admission_failures=%u surface_hits=%u sky_hits=%u unknown_hits=%u unsupported_hits=%u transport_rays=%u direct_rays=%u occupied_rows=%u initialized_rows=%u row_refreshes=%u resident_admission_failures=%u frames_since_reset=%llu eviction_passes=%llu gpu_bytes=%llu scheduled_rows=%u ready_rows=%u selection_occupied=%u selection_start=%u selection_wraps=%u player_visibility_calls=%u player_history_visibility_calls=%u pinned_rows=%u contributor_limit=%u mandatory_ready=%u contributor_admission_failures=%u work_cumulative_since_reset=1 occupancy_live=1\n",
        static_cast<unsigned long long>(r.frames),static_cast<unsigned long long>(s.stats.measured_frame),
        s.stats.measured_epoch,s.epoch,s.radiance_epoch,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],c[11],
        static_cast<unsigned long long>(s.stats.frames_since_reset),static_cast<unsigned long long>(s.stats.eviction_passes),
        static_cast<unsigned long long>(s.stats.gpu_bytes),s.stats.scheduled_rows,s.stats.ready_rows,
        s.stats.selection_occupied,s.stats.selection_start,s.stats.selection_wraps,
        s.stats.player_visibility_calls,s.stats.player_history_visibility_calls,s.stats.pinned_rows,s.stats.contributor_limit,
        s.stats.mandatory_ready?1u:0u,c[1]-c[11]);
  }
  return true;
}
}
