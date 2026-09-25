#pragma once
#include <memory>
#include <cstdint>
namespace rhi {class IBuffer;}
namespace octaryn::client::world_presentation {struct StreamColumn;}
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldColumnGpu;
struct WorldMeshJobResources {
  std::uint64_t buffers_created{},fences_created{},input_capacity{},scratch_bytes{};
  std::uint64_t signal_value{},observed_value{};
  std::uint64_t poll_calls{},poll_timeouts{};
  std::uint64_t empty_fluid_reuses{};
  std::int32_t last_poll_result{};
  double allocation_worker_ms{};
  bool emitting{},finished{},allocating{};
};
// A single exact-size GPU mesh, advanced without host waits by its owner.
class WorldMeshJob {
  struct State;
  std::unique_ptr<State> state_;
public:
  WorldMeshJob();
  ~WorldMeshJob();
  bool start(WorldRenderer&,const world_presentation::StreamColumn&);
  bool poll(WorldRenderer&,WorldColumnGpu&,bool& complete);
  bool wait(std::uint64_t timeout=UINT64_MAX); // Teardown and explicit qualification only; streaming polls with zero timeout.
  // Borrowed after completed emit; invalidated by the next start or destruction.
  rhi::IBuffer* finished_counters() const;
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources() const;
};
}
