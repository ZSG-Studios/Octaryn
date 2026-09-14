#pragma once
#include <memory>
#include <cstdint>
namespace octaryn::client::world_presentation {struct StreamColumn;}
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldColumnGpu;
struct WorldMeshJobResources {
  std::uint64_t buffers_created{},fences_created{},input_capacity{},scratch_bytes{};
  std::uint64_t signal_value{},observed_value{};
  std::uint64_t poll_calls{},poll_timeouts{};
  std::int32_t last_poll_result{};
  bool emitting{},finished{};
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
  std::uint64_t gpu_bytes() const;
  WorldMeshJobResources resources() const;
};
}
