#pragma once
#include "WorldRenderer.h"
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace octaryn::client::app {
// Explicit diagnostic camera motion never alters the authoritative player.
class LightingMovementValidation {
public:
  bool camera(rendering::WorldCamera& camera,const rendering::WorldRendererStats& stats,unsigned radius) {
    if(!started_) {
      const auto expected=(2*radius+1)*(2*radius+1);
      if(stats.columns!=expected || stats.pending_meshes || stats.ray_pending_columns) {
        stable_frames_=0;previous_frame_=stats.frames;return false;
      }
      if(stats.frames!=previous_frame_){++stable_frames_;previous_frame_=stats.frames;}
      if(stable_frames_<128)return false;
      origin_=camera;start_frame_=stats.frames;started_=true;
      std::printf("lighting_motion fixture=lateral_sine amplitude_m=4 period_frames=256 start_frame=%llu authority=unchanged os_events_injected=0\n",
          static_cast<unsigned long long>(start_frame_));
      std::fflush(stdout);
    }
    camera=origin_;
    camera.x+=4*std::sin(float(stats.frames-start_frame_)*6.28318530718f/256);
    return true;
  }
private:
  rendering::WorldCamera origin_{};
  std::uint64_t previous_frame_{},start_frame_{};
  unsigned stable_frames_{};
  bool started_{};
};
}
