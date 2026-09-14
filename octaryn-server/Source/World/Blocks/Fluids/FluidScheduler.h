#pragma once
#include "FluidEvaluator.h"
#include <map>
#include <set>
#include <tuple>

namespace octaryn::server::world::blocks {
inline constexpr std::size_t MaxPendingFluids=8192;
struct FluidRegion {
  std::int32_t center_x{},center_z{};
  std::uint32_t radius{};
  bool operator==(const FluidRegion&) const=default;
};
enum class FluidApplyResult {Applied,Unchanged,Retry};
// Callback checks expected against current authoritative state, atomically commits
// its complete support cascade, persists/replicates it, or returns Retry unchanged.
using FluidApply=std::function<FluidApplyResult(BlockPosition,std::uint16_t,std::uint16_t)>;
struct FluidTickBudget {
  std::uint32_t evaluations{256},applies{128},reads{65536};
  std::uint32_t repair_samples{4096},repair_schedules{64};
  std::uint32_t max_time_us{2000}; // Zero disables wall time for deterministic tests.
};
struct FluidTickReport {
  std::uint32_t evaluations{},applies{},changed{},retries{},reads{};
  std::uint32_t repair_samples{},repair_schedules{};
  std::uint32_t repair_deferred{}; // Twice unavailable: retired to cyclic repair.
  std::uint32_t pending{};
  std::uint64_t saturated{}; // Cumulative admission failures, recovered by repair.
  bool budget_exhausted{};
};
class FluidScheduler {
public:
  // Single authority thread, stable callback view during each evaluation. The
  // simulation square has a read-only one-column halo; nothing is applied there.
  // Two unavailable evaluations retire a proposal to the continuous repair scan.
  // Apply Retry (replication backpressure/stale result) is never retired.
  explicit FluidScheduler(FluidRules rules);
  bool set_region(FluidRegion region);
  bool schedule(BlockPosition position,std::uint64_t due_ms);
  void notify_change(BlockPosition position,std::uint16_t before,
                     std::uint16_t after,std::uint64_t now_ms);
  FluidTickReport tick(std::uint64_t now_ms,const FluidRead& read,
                       const FluidApply& apply,FluidTickBudget budget={});
  std::size_t pending_count() const {return pending_.size();}
private:
  using Position=std::tuple<std::int32_t,std::int32_t,std::int32_t>;
  using Due=std::pair<std::uint64_t,Position>;
  FluidRules rules_;
  FluidRegion region_{};
  bool configured_{};
  struct Pending {std::uint64_t due;std::uint32_t unavailable{};};
  std::map<Position,Pending> pending_;
  std::set<Due> due_;
  std::uint64_t repair_cursor_{},saturated_{},last_now_{};
  bool contains(BlockPosition position) const;
  bool sample_contains(BlockPosition position) const;
  void neighborhood(BlockPosition position,std::uint64_t due_ms);
  BlockPosition repair_position() const;
  std::uint64_t region_volume() const;
};
}
