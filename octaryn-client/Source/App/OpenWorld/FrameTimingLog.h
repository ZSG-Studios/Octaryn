#pragma once
#include "FrameProfile.h"
#include "WorldRenderer.h"
#include "../../Threading/ThreadCpuTime.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>

namespace octaryn::client::app {
class FrameTimingLog {
public:
  explicit FrameTimingLog(const char* path) {
    if(!path || !*path)return;
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open per-frame timing log");
    output_<<std::setprecision(9)
        <<"frame,total_ms,session_ms,stream_ms,render_ms,ui_ms,events_ms,cap_sleep_ms,columns,pending_meshes,"
          "ray_pending,eye_x,eye_y,eye_z,yaw,pitch,fov,gi_ready,main_thread_cpu_ms\n";
    if(live_)output_.flush();
  }

  void frame(const frame_profile_sample& sample,const rendering::WorldRendererStats& stats,
      const rendering::WorldCamera& camera) {
    if(!output_.is_open())return;
    const auto cpu_time=threading::current_thread_cpu_nanoseconds();
    const double cpu_ms=last_cpu_time_>=0 && cpu_time>=last_cpu_time_
        ?double(cpu_time-last_cpu_time_)/1'000'000.0:-1;
    last_cpu_time_=cpu_time;
    output_<<stats.frames<<','<<sample.total_ms<<','<<sample.sim_ms<<','<<sample.world_ms<<','
        <<sample.render_ms<<','<<sample.ui_ms<<','<<sample.misc_ms<<','<<sample.fps_cap_sleep_ms<<','
        <<stats.map_primitives<<','
        <<camera.x<<','<<camera.y<<','<<camera.z<<','<<camera.yaw<<','<<camera.pitch<<','
        <<camera.vertical_fov<<','<<(stats.gi_ready?1:0)<<','<<cpu_ms<<'\n';
    if(live_)output_.flush();
  }

private:
  std::ofstream output_;
  std::int64_t last_cpu_time_{-1};
  bool live_=std::getenv("OCTARYN_CLIENT_LIVE_FRAME_TIMING")!=nullptr;
};
}
