#pragma once
#include "WorldMeshJob.h"
#include <cstddef>
#include <memory>
namespace octaryn::client::world_presentation {class WorldStream;}
namespace octaryn::client::rendering {
struct WorldRenderer;
// Two staged meshes, ordered query publication, and one new count per pump.
class WorldDeliveryJobs {
  struct State;
  std::unique_ptr<State> state_;
public:
  WorldDeliveryJobs();
  ~WorldDeliveryJobs();
  bool pump(WorldRenderer&,world_presentation::WorldStream&);
  bool progress(WorldRenderer&);
  void retain_window(const WorldRenderer&);
  std::size_t pending() const;
  std::size_t completed() const;
  std::size_t cancelled() const;
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources(std::size_t slot) const;
  bool wait(std::size_t slot,std::uint64_t timeout); // Explicit qualification only.
};
}
