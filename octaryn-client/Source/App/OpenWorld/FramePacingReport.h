#pragma once

#include "FramePacing.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace octaryn::client::app {
class FramePacingReport {
public:
  explicit FramePacingReport(const char* path = nullptr) {
    if(!path || !*path)return;
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open frame pacing profile");
    output_ << "frame,frame_ms,requested_sleep_ms,actual_sleep_ms,columns,pending_meshes\n";
  }

  void frame(std::uint64_t requested_ns, float sleep_ms, float total_ms,
      unsigned columns, unsigned pending_meshes) {
    ++frames_;
    sleeps_ += requested_ns > 0;
    requested_ms_ += double(requested_ns) / 1e6;
    sleep_ms_ += sleep_ms;
    total_ms_ += total_ms;
    worst_ms_ = std::max(worst_ms_, total_ms);
    if(output_.is_open())output_ << frames_ << ',' << total_ms << ',' << double(requested_ns)/1e6
        << ',' << sleep_ms << ',' << columns << ',' << pending_meshes << '\n';
  }

  void report(const FramePacing& pacing, unsigned cap, bool vsync) const {
    const double count = frames_ ? double(frames_) : 1.0;
    std::printf("frame_pacing_summary frames=%u cap=%u target_fps=%.3f refresh_hz=%.3f "
        "vsync=%u display_queries=%u sleeps=%u requested_sleep_avg_ms=%.3f "
        "actual_sleep_avg_ms=%.3f frame_avg_ms=%.3f frame_worst_ms=%.3f\n",
        frames_, cap, pacing.target_hz(cap), pacing.refresh_hz(), vsync ? 1u : 0u,
        pacing.display_queries(), sleeps_, requested_ms_ / count, sleep_ms_ / count,
        total_ms_ / count, worst_ms_);
  }

private:
  std::ofstream output_;
  unsigned frames_{}, sleeps_{};
  double requested_ms_{}, sleep_ms_{}, total_ms_{};
  float worst_ms_{};
};
}
