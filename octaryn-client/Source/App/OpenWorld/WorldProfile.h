#pragma once

#include "FrameProfile.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <filesystem>
#include <array>

namespace octaryn::client::app {
class WorldProfile {
public:
  explicit WorldProfile(const std::filesystem::path& path);
  ~WorldProfile();
  void frame(SDL_Window* window, const frame_profile_sample& sample,
             const LocalPlayerPose& pose, const rendering::WorldRendererStats& renderer,
             const char* state, LocalMovementStats movement);
  frame_profile_snapshot snapshot() const;
  void restart_measurement();
  void report_slow_frames() const;
private:
  FILE* file_{};
  frame_metrics metrics_{};
  uint64_t last_report_{};
  uint64_t frames_{};
  frame_profile_sample latest_{};
  double sim_total_{}, world_total_{}, render_total_{}, ui_total_{};
  uint64_t report_samples_{};
  struct SlowFrame { uint64_t frame{}; frame_profile_sample sample{}; };
  std::array<SlowFrame, 8> slow_frames_{};
};
}
