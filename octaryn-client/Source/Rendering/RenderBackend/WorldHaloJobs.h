#pragma once
#include <memory>
#include "WorldMeshJob.h"
#include <utility>
#include <cstdint>
#include <cstddef>
namespace octaryn::client::rendering {
struct WorldRenderer;
class WorldHaloJobs {
  struct State;
  std::unique_ptr<State> state_;
public:
  static constexpr std::size_t Capacity=8;
  WorldHaloJobs();
  ~WorldHaloJobs();
  bool pump(WorldRenderer&,double start_budget_ms=6.0,double progress_budget_ms=2.0);
  // Advances submitted phases only; completed geometry stays private until pump.
  bool progress(WorldRenderer&,double budget_ms=2.0);
  bool contains(std::pair<std::int32_t,std::int32_t>) const;
  std::size_t pending() const;
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources(std::size_t slot) const;
  bool current(std::size_t slot,const WorldRenderer&) const;
  bool wait(std::size_t slot,std::uint64_t timeout); // Explicit qualification only; pump never calls this.
};
bool world_mesh_has_pending(const WorldRenderer&);
}
