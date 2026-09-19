#pragma once
#include "WorldRenderer.h"
#include "WorldStream.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <stdexcept>

namespace octaryn::client::app {
// Explicit diagnostic camera motion never alters the authoritative player.
class LightingMovementValidation {
public:
  bool camera(rendering::WorldCamera& camera,const rendering::WorldRendererStats& stats,unsigned radius,
      const world_presentation::WorldStream& stream) {
    if(!started_) {
      const auto expected=(2*radius+1)*(2*radius+1);
      if(stats.columns!=expected || stats.pending_meshes || stats.ray_pending_columns) {
        stable_frames_=0;previous_frame_=stats.frames;return false;
      }
      if(stats.frames!=previous_frame_){++stable_frames_;previous_frame_=stats.frames;}
      if(stable_frames_<128)return false;
      origin_=camera;
      unsigned clearance{};
      for(clearance=2;clearance<=64;++clearance) {
        origin_.y=camera.y+float(clearance);
        if(path_clear(stream))break;
      }
      if(clearance>64)throw std::runtime_error("No clear lighting-camera diagnostic path within 64 blocks");
      start_frame_=stats.frames;started_=true;
      std::printf("lighting_motion fixture=lateral_sine amplitude_m=4 period_frames=256 clearance_m=%u start_frame=%llu authority=unchanged os_events_injected=0\n",
          clearance,static_cast<unsigned long long>(start_frame_));
      std::fflush(stdout);
    }
    camera=origin_;
    camera.x+=4*std::sin(float(stats.frames-start_frame_)*6.28318530718f/256);
    return true;
  }
private:
  bool path_clear(const world_presentation::WorldStream& stream) const {
    for(unsigned frame=0;frame<256;++frame) {
      const float x=origin_.x+4*std::sin(float(frame)*6.28318530718f/256);
      for(int z=int(std::floor(origin_.z-.35f));z<=int(std::floor(origin_.z+.35f));++z)
        for(int y=int(std::floor(origin_.y-.35f));y<=int(std::floor(origin_.y+.35f));++y)
          for(int column=int(std::floor(x-.35f));column<=int(std::floor(x+.35f));++column) {
            std::uint16_t block{};
            if(!stream.try_block(column,y,z,block) || block!=0)return false;
          }
    }
    return true;
  }
  rendering::WorldCamera origin_{};
  std::uint64_t previous_frame_{},start_frame_{};
  unsigned stable_frames_{};
  bool started_{};
};
}
