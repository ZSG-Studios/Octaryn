#include "TileSessionInternal.h"
#include "TileRayWorkBudget.h"
#include "MapRayBudget.h"
#include <cstdio>

namespace octaryn::client::rendering {
bool TileSession::State::reserve_ray(unsigned index) {
  auto& entry=entries[index];if(entry.ray_budget_checked)return true;
  std::uint64_t ray_bytes{};
  if(!map_ray_build_budget(*entry.map,ray_bytes)){error="tile_ray_size_query_failed";return false;}
  entry.reserved=map_memory_stats(*entry.map).geometry+ray_bytes+65536;
  entry.ray_budget_checked=true;refresh();
  const auto required=statistics.resident_bytes+statistics.reserved_bytes+statistics.retired_bytes+statistics.texture_bytes;
  if(required>budget.gpu_bytes) {
    error="tile_ray_working_set_exceeds_gpu_budget tile="+std::to_string(index)+
        " required_bytes="+std::to_string(required)+" budget_bytes="+std::to_string(budget.gpu_bytes);
    return false;
  }
  std::printf("tile_ray_budget id=%u peak_bytes=%llu working_set_bytes=%llu budget_bytes=%llu\n",index,
      static_cast<unsigned long long>(ray_bytes),static_cast<unsigned long long>(required),
      static_cast<unsigned long long>(budget.gpu_bytes));return true;
}
bool TileSession::State::publish_ray(unsigned index) {
  auto& entry=entries[index];entry.ray_active=false;
  if(entry.phase==Phase::Ready)return true;
  if(!collision->set_tile(index,std::move(entry.collision))) {error="tile_collision_publish_failed";return false;}
  entry.phase=Phase::Ready;tiles.mutable_tile(index)->resident=true;
  ++statistics.published;changed=true;
  std::printf("tile_published id=%u file=%s collision_ready=1 ray_ready=%u readiness_ms=%.3f deadline_missed=%u\n",index,
      tiles.tile(index)->file.c_str(),map_ray_ready(*entry.map)?1u:0u,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-entry.requested_at).count(),
      entry.deadline_reported?1u:0u);return true;
}
bool TileSession::State::progress_rays(bool ray_required,const MapRaySubmitScope* profile) {
  const auto started=std::chrono::steady_clock::now();
  const auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
  TileRayWorkBudget limit{budget.upload_ms,ray_capacity};
  unsigned in_flight=0,cancelled=0,published=0,wait_fences=0,wait_allocations=0;
  // Cancellation never waits. Pending external submissions/allocation retain the
  // map in retirement and continue occupying one of the four lifecycle slots.
  for(unsigned index:priority) {
    auto& entry=entries[index];
    if(entry.phase==Phase::Ray && !entry.wanted && cancelled<limit.capacity) {
      retired.push_back({std::move(entry.map),last_fence,last_signal,entry.reserved});
      if(entry.ray_active)++retired_ray_pending;
      entry.collision.reset();entry.phase=Phase::Absent;entry.reserved=0;
      entry.ray_budget_checked=entry.ray_active=false;++statistics.cancelled;++cancelled;
    }
    if(entry.ray_active)++in_flight;
  }
  // Reuse this pump's retirement pass; do not add another unbounded fence scan.
  in_flight+=retired_ray_pending;
  if(in_flight>limit.capacity) {error="tile_ray_inflight_limit_exceeded";return false;}
  struct Candidate {unsigned index;MapRayStep step;};
  std::array<Candidate,TileRayWorkBudget::max_capacity> candidates{};unsigned count=0;
  for(unsigned index:priority) {
    auto& entry=entries[index];if(!entry.ray_active)continue;
    if(!limit.poll()) {error="tile_ray_poll_limit_exceeded";return false;}
    const auto step=poll_map_ray_scene(*entry.map);
    if(step==MapRayStep::Ready) {
      if(!publish_ray(index))return false;--in_flight;++published;
    } else {
      wait_fences+=step==MapRayStep::BuildFence || step==MapRayStep::CompactFence;
      wait_allocations+=step==MapRayStep::CompactAllocation;
      candidates[count++]={index,step};
    }
  }
  unsigned worked=UINT32_MAX;MapRayStep work_step=MapRayStep::Ready;double work_ms=0;
  const auto perform=[&](unsigned index,MapRayStep step) {
    auto& entry=entries[index];
    if(!limit.allows(step,in_flight,elapsed()))return true;
    if(!reserve_ray(index))return false;
    if(!limit.allows(step,in_flight,elapsed()))return true;
    const auto work_started=std::chrono::steady_clock::now();
    if(!perform_map_ray_work(*entry.map,queue,step,profile)) {error="tile_ray_build_failed";return false;}
    work_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-work_started).count();
    limit.record(step);worked=index;work_step=step;
    if(!entry.ray_active) {entry.ray_active=true;++in_flight;}
    return true;
  };
  // Existing lifecycles get first refusal, but a pending fence/future never
  // prevents another ready lifecycle from using the single operation allowance.
  for(unsigned i=0;i<count && !limit.operations;++i)
    if(!perform(candidates[i].index,candidates[i].step))return false;
  if(!limit.operations && in_flight<limit.capacity)for(unsigned index:priority) {
    auto& entry=entries[index];
    if(entry.ray_active || !entry.map || !entry.wanted)continue;
    if(entry.phase!=Phase::Ray && !(entry.phase==Phase::Ready && ray_required && !map_ray_ready(*entry.map)))continue;
    if(!limit.poll())break;
    if(!ray_required) {if(!publish_ray(index))return false;++published;break;}
    const auto step=poll_map_ray_scene(*entry.map);
    if(step==MapRayStep::Ready) {if(!publish_ray(index))return false;++published;}
    else if(!perform(index,step))return false;
    break;
  }
  ray_polls+=limit.polls;ray_wait_fences+=wait_fences;ray_wait_allocations+=wait_allocations;
  ray_operations+=limit.operations;ray_submissions+=limit.submissions;
  if(limit.polls || cancelled || frame%120==0)
    std::printf("tile_ray_schedule frame=%llu polls=%u wait_fences=%u wait_allocations=%u operations=%u submissions=%u inflight=%u capacity=%u published=%u cancelled=%u work_tile=%u work_step=%u work_ms=%.3f cpu_ms=%.3f soft_budget_ms=%.3f total_polls=%llu total_wait_fences=%llu total_wait_allocations=%llu total_operations=%llu total_submissions=%llu\n",
        static_cast<unsigned long long>(frame),limit.polls,wait_fences,wait_allocations,limit.operations,limit.submissions,
        in_flight,limit.capacity,published,cancelled,worked,unsigned(work_step),work_ms,elapsed(),budget.upload_ms,
        static_cast<unsigned long long>(ray_polls),static_cast<unsigned long long>(ray_wait_fences),
        static_cast<unsigned long long>(ray_wait_allocations),static_cast<unsigned long long>(ray_operations),
        static_cast<unsigned long long>(ray_submissions));
  return true;
}
}
