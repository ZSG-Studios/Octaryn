#pragma once
#include "WorldMeshJob.h"
#include <cstddef>
#include <memory>
namespace octaryn::client::world_presentation {class WorldStream;}
namespace octaryn::client::rendering {
struct WorldRenderer;
// Fixed staging capacity with ordered query publication and budgeted dispatch.
class WorldDeliveryJobs {
  struct State;
  std::unique_ptr<State> state_;
public:
  static constexpr std::size_t Capacity=8;
  WorldDeliveryJobs();
  ~WorldDeliveryJobs();
  bool pump(WorldRenderer&,world_presentation::WorldStream&,double budget_ms=2.0);
  bool progress(WorldRenderer&,double budget_ms=2.0);
  // Advance/fill private slots only; publication remains at the frame head.
  bool prefetch(WorldRenderer&,world_presentation::WorldStream&,double budget_ms);
  void retain_window(const WorldRenderer&);
  std::size_t pending() const;
  std::size_t completed() const;
  std::size_t cancelled() const;
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources(std::size_t slot) const;
  bool wait(std::size_t slot,std::uint64_t timeout); // Explicit qualification only.
};
}
