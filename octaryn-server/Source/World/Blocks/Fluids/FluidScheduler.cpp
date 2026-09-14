#include "FluidScheduler.h"
#include "FluidSampling.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace octaryn::server::world::blocks {
namespace {
std::uint64_t later(std::uint64_t now,std::uint64_t delay) {
  return now>UINT64_MAX-delay?UINT64_MAX:now+delay;
}
BlockPosition position_of(const std::tuple<std::int32_t,std::int32_t,std::int32_t>& key) {
  return {std::get<0>(key),std::get<1>(key),std::get<2>(key)};
}
// Original water.cpp:537-563. Repair wakes only contacts, spreading donors,
// or unsupported flowing levels; stable enclosed sources are not repair work.
bool needs_repair(BlockPosition p,std::uint16_t current,const FluidRules& rules,
                  const FluidRead& read) {
  const auto kind=rules.kind(current);
  if(kind==FluidKind::None) return false;
  std::uint16_t next{};
  if(!evaluate_fluid(p,rules,read,next)) return false;
  if(kind==FluidKind::Lava && next==rules.stone) return true;
  fluid_detail::Sampling sampling{rules,read};
  if(sampling.downward(p,kind) && sampling.available) return true;
  for(int direction=0;sampling.available && direction<4;++direction)
    if(sampling.donor_spreads(p,direction,kind) && sampling.available) return true;
  return sampling.available && !rules.source(current) && next!=current;
}
}

FluidScheduler::FluidScheduler(FluidRules rules):rules_(std::move(rules)) {
  if(!validate_fluid_rules(rules_)) throw std::invalid_argument("Invalid fluid rules");
}
bool FluidScheduler::contains(BlockPosition p) const {
  if(!configured_ || p.y<WorldMinY || p.y>=WorldMaxYExclusive) return false;
  const auto low_x=(std::int64_t(region_.center_x)-region_.radius)*32;
  const auto low_z=(std::int64_t(region_.center_z)-region_.radius)*32;
  const auto width=(std::int64_t(region_.radius)*2+1)*32;
  return p.x>=low_x && p.x<low_x+width && p.z>=low_z && p.z<low_z+width;
}
bool FluidScheduler::set_region(FluidRegion region) {
  if(region.radius>32) return false;
  for(const auto center:{region.center_x,region.center_z}) {
    const auto low=(std::int64_t(center)-region.radius-1)*32;
    const auto high=(std::int64_t(center)+region.radius+2)*32-1;
    if(low<INT32_MIN || high>INT32_MAX) return false;
  }
  if(configured_ && region==region_) return true;
  region_=region;configured_=true;repair_cursor_=0;
  for(auto it=pending_.begin();it!=pending_.end();) {
    if(contains(position_of(it->first))) {++it;continue;}
    due_.erase({it->second.due,it->first});it=pending_.erase(it);
  }
  return true;
}
bool FluidScheduler::sample_contains(BlockPosition p) const {
  if(!configured_ || p.y<WorldMinY || p.y>=WorldMaxYExclusive) return false;
  const auto low_x=(std::int64_t(region_.center_x)-region_.radius-1)*32;
  const auto low_z=(std::int64_t(region_.center_z)-region_.radius-1)*32;
  const auto width=(std::int64_t(region_.radius)*2+3)*32;
  return p.x>=low_x && p.x<low_x+width && p.z>=low_z && p.z<low_z+width;
}
bool FluidScheduler::schedule(BlockPosition p,std::uint64_t due_ms) {
  if(!contains(p)) return false;
  const Position key{p.x,p.y,p.z};
  auto found=pending_.find(key);
  if(found!=pending_.end()) {
    if(due_ms<found->second.due) {
      auto node=due_.extract({found->second.due,key});
      node.value().first=due_ms;due_.insert(std::move(node));found->second.due=due_ms;
    }
    return true;
  }
  if(pending_.size()==MaxPendingFluids) {++saturated_;return false;}
  const auto inserted=pending_.emplace(key,Pending{due_ms}).first;
  try {due_.insert({due_ms,key});}
  catch(...) {pending_.erase(inserted);throw;}
  return true;
}
void FluidScheduler::neighborhood(BlockPosition p,std::uint64_t due_ms) {
  static constexpr int offsets[7][3]={{0,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,0,0},{-1,0,0}};
  for(const auto& delta:offsets) {
    const auto x=std::int64_t(p.x)+delta[0],y=std::int64_t(p.y)+delta[1],z=std::int64_t(p.z)+delta[2];
    if(x<INT32_MIN || x>INT32_MAX || y<WorldMinY || y>=WorldMaxYExclusive || z<INT32_MIN || z>INT32_MAX) continue;
    schedule({static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),static_cast<std::int32_t>(z)},due_ms);
  }
}
void FluidScheduler::notify_change(BlockPosition p,std::uint16_t before,
                                    std::uint16_t after,std::uint64_t now_ms) {
  const auto kind=rules_.kind(after)!=FluidKind::None?rules_.kind(after):rules_.kind(before);
  const bool contact=after==rules_.stone && rules_.kind(before)==FluidKind::Lava;
  const auto delay=contact?0u:kind==FluidKind::Lava?500u:250u;
  neighborhood(p,later(std::max(now_ms,last_now_),delay));
}
std::uint64_t FluidScheduler::region_volume() const {
  const auto width=(std::uint64_t(region_.radius)*2+1)*32;
  return width*width*(WorldMaxYExclusive-WorldMinY);
}
BlockPosition FluidScheduler::repair_position() const {
  constexpr std::uint64_t height=WorldMaxYExclusive-WorldMinY,column_volume=32*height*32;
  const auto columns=std::uint64_t(region_.radius)*2+1;
  const auto column=repair_cursor_/column_volume,local=repair_cursor_%column_volume;
  return {static_cast<std::int32_t>((std::int64_t(region_.center_x)-region_.radius)*32+
              static_cast<std::int64_t>((column%columns)*32+local%32)),
          static_cast<std::int32_t>(WorldMinY+static_cast<std::int64_t>((local/32)%height)),
          static_cast<std::int32_t>((std::int64_t(region_.center_z)-region_.radius)*32+
              static_cast<std::int64_t>((column/columns)*32+local/(32*height)))};
}
FluidTickReport FluidScheduler::tick(std::uint64_t now_ms,const FluidRead& read,
                                     const FluidApply& apply,FluidTickBudget budget) {
  FluidTickReport report{};
  const auto finish=[&] {report.pending=static_cast<std::uint32_t>(pending_.size());report.saturated=saturated_;return report;};
  if(!configured_ || !read || !apply) return finish();
  now_ms=std::max(now_ms,last_now_);last_now_=now_ms;
  budget.evaluations=std::min(budget.evaluations,256u);budget.applies=std::min(budget.applies,128u);
  budget.reads=std::min(budget.reads,65536u);budget.repair_samples=std::min(budget.repair_samples,4096u);
  budget.repair_schedules=std::min(budget.repair_schedules,64u);
  const auto started=std::chrono::steady_clock::now();
  const auto exhausted=[&] {
    return report.reads>=budget.reads || (budget.max_time_us!=0 &&
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count()>=budget.max_time_us);
  };
  const FluidRead bounded_read=[&](const BlockPosition& p,std::uint16_t& block) {
    if(!sample_contains(p) || report.reads>=budget.reads) return false;
    ++report.reads;return read(p,block);
  };
  const auto erase=[&](const Position& key) {
    const auto it=pending_.find(key);
    if(it!=pending_.end()) {due_.erase({it->second.due,key});pending_.erase(it);}
  };
  while(!due_.empty() && due_.begin()->first<=now_ms && report.evaluations<budget.evaluations) {
    if(exhausted()) {report.budget_exhausted=true;break;}
    const auto key=due_.begin()->second;const auto p=position_of(key);
    std::uint16_t current{},next{};
    ++report.evaluations;
    if(!bounded_read(p,current) || !evaluate_fluid(p,rules_,bounded_read,next)) {
      ++report.retries;
      if(report.reads>=budget.reads) {report.budget_exhausted=true;break;}
      const auto failures=pending_.at(key).unavailable+1;
      erase(key);
      if(failures>=2) ++report.repair_deferred;
      else if(schedule(p,later(now_ms,250))) pending_.at(key).unavailable=failures;
      continue;
    }
    if(current==next) {erase(key);continue;}
    if(report.applies>=budget.applies) {report.budget_exhausted=true;break;}
    ++report.applies;
    const auto result=apply(p,current,next);
    erase(key);
    if(result==FluidApplyResult::Retry) {++report.retries;schedule(p,later(now_ms,250));}
    else if(result==FluidApplyResult::Applied) {++report.changed;notify_change(p,current,next,now_ms);}
  }
  if(!due_.empty() && due_.begin()->first<=now_ms && report.evaluations>=budget.evaluations)
    report.budget_exhausted=true;
  // Cyclic repair has no secondary overflow list. Original repair intentionally
  // wakes qualifying work immediately, even if an event had a later deadline.
  while(report.repair_samples<budget.repair_samples && report.repair_schedules<budget.repair_schedules) {
    if(exhausted()) {report.budget_exhausted=true;break;}
    const auto p=repair_position();
    ++report.repair_samples;std::uint16_t block{};
    bool repair=false;
    if(bounded_read(p,block) && rules_.kind(block)!=FluidKind::None) {
      if(report.evaluations>=budget.evaluations) {report.budget_exhausted=true;break;}
      ++report.evaluations;
      repair=needs_repair(p,block,rules_,bounded_read);
    }
    if(!repair && report.reads>=budget.reads) {report.budget_exhausted=true;break;}
    repair_cursor_=(repair_cursor_+1)%region_volume();
    if(repair) {
      neighborhood(p,now_ms);++report.repair_schedules;
    }
  }
  return finish();
}
}
