#pragma once
#include "FrameProfile.h"
#include "WorldRenderer.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace octaryn::client::app {
class FrameTimingLog {
public:
  explicit FrameTimingLog(const char* path) {
    if(!path || !*path)return;
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open per-frame timing log");
    output_<<std::setprecision(9)
        <<"frame,total_ms,session_ms,stream_ms,render_ms,ui_ms,events_ms,cap_sleep_ms,columns,pending_meshes,"
          "ray_pending,eye_x,eye_y,eye_z,yaw,pitch,fov\n";
  }

  void frame(const frame_profile_sample& sample,const rendering::WorldRendererStats& stats,
      const rendering::WorldCamera& camera) {
    if(!output_.is_open())return;
    output_<<stats.frames<<','<<sample.total_ms<<','<<sample.sim_ms<<','<<sample.world_ms<<','
        <<sample.render_ms<<','<<sample.ui_ms<<','<<sample.misc_ms<<','<<sample.fps_cap_sleep_ms<<','
        <<stats.columns<<','<<stats.pending_meshes<<','<<stats.ray_pending_columns<<','
        <<camera.x<<','<<camera.y<<','<<camera.z<<','<<camera.yaw<<','<<camera.pitch<<','
        <<camera.vertical_fov<<'\n';
  }

private:
  std::ofstream output_;
};
}
