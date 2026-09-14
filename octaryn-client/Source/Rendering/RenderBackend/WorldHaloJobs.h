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
  WorldHaloJobs();
  ~WorldHaloJobs();
  bool pump(WorldRenderer&);
  bool contains(std::pair<std::int32_t,std::int32_t>) const;
  std::size_t pending() const;
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources(std::size_t slot) const;
  bool current(std::size_t slot,const WorldRenderer&) const;
  bool wait(std::size_t slot,std::uint64_t timeout); // Explicit qualification only; pump never calls this.
};
bool world_mesh_has_pending(const WorldRenderer&);
}
