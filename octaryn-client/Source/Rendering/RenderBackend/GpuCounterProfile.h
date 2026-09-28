#pragma once
#include <slang-rhi.h>
#include <cstdint>
#include <memory>
#include <array>

namespace octaryn::client::rendering {
struct GpuCounterObservation {
  std::array<float,6> camera{}; // x,y,z,yaw,pitch,vertical FOV; unjittered.
  std::array<int,4> dimensions{}; // render width/height, output width/height.
  std::uint64_t ready_frame{},sampling_frame{};
  std::int64_t reflection_sampling_frame{-1};
  float delta_ms{};
  bool fixed_sampling{};
};
// Diagnostic only. No instance, DLL or recording callback exists when disabled.
class GpuCounterProfile {
public:
  struct State;
  static std::unique_ptr<GpuCounterProfile> create(bool dx12);
  ~GpuCounterProfile();
  void attach(rhi::IDevice* device);
  void begin(rhi::ICommandEncoder* commands,std::uint64_t frame,bool eligible,std::uint64_t scene_revision);
  void end(rhi::ICommandEncoder* commands,const GpuCounterObservation& observation);
  void submitted(rhi::IFence* fence,std::uint64_t signal);
  void poll();
  // Caller must first drain the renderer queue, including abandoned submissions.
  void shutdown();
private:
  explicit GpuCounterProfile(State* state):state_(state) {}
  State* state_{};
};
}
