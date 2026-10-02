#pragma once
#include "FrameProfile.h"
#include "WorldRenderer.h"
#include "../../Threading/ThreadCpuTime.h"
#include "../../Diagnostics/AsyncProfileStream.h"
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
        <<"frame,total_ms,sim_ms,world_ms,render_ms,ui_ms,events_ms,cap_sleep_ms,map_primitives,"
          "eye_x,eye_y,eye_z,yaw,pitch,fov,gi_ready,main_thread_cpu_ms,schema_version,"
          "allocated_gpu_estimate_bytes,map_texture_bytes,map_geometry_bytes,map_acceleration_bytes,map_scratch_bytes,"
          "gpu_local_usage_bytes,gpu_local_budget_bytes,gpu_budget_available,process_resident_bytes,process_peak_bytes,"
          "render_width,render_height,display_width,display_height,upscaler_mode,render_scale,dynamic_resolution,ray_tracing_active,"
          "world_items,awake_world_items,item_assets,phase\n";
    if(live_)output_.flush();
  }

  void frame(const frame_profile_sample& sample,const rendering::WorldRendererStats& stats,
      const rendering::WorldCamera& camera,const char* phase="scene") {
    if(!output_.is_open())return;
    const auto cpu_time=threading::current_thread_cpu_nanoseconds();
    const double cpu_ms=last_cpu_time_>=0 && cpu_time>=last_cpu_time_
        ?double(cpu_time-last_cpu_time_)/1'000'000.0:-1;
    last_cpu_time_=cpu_time;
    // Renderer stats count completed frames; GPU/capture records use zero-based IDs.
    output_<<(stats.frames?stats.frames-1:0)<<','<<sample.total_ms<<','<<sample.sim_ms<<','<<sample.world_ms<<','
        <<sample.render_ms<<','<<sample.ui_ms<<','<<sample.misc_ms<<','<<sample.fps_cap_sleep_ms<<','
        <<stats.map_primitives<<','
        <<camera.x<<','<<camera.y<<','<<camera.z<<','<<camera.yaw<<','<<camera.pitch<<','
        <<camera.vertical_fov<<','<<(stats.gi_ready?1:0)<<','<<cpu_ms<<",3,"
        <<stats.gpu_bytes<<','<<stats.map_texture_bytes<<','<<stats.map_geometry_bytes<<','
        <<stats.map_acceleration_bytes<<','<<stats.map_scratch_bytes<<','
        <<stats.gpu_local_usage<<','<<stats.gpu_local_budget<<','<<(stats.gpu_budget_available?1:0)<<','
        <<stats.process_resident_bytes<<','<<stats.process_peak_bytes<<','
        <<stats.render_width<<','<<stats.render_height<<','<<stats.display_width<<','<<stats.display_height<<','
        <<stats.upscaler_mode<<','<<stats.fsr_render_scale<<','<<(stats.fsr_dynamic_active?1:0)<<','
        <<(stats.ray_tracing_active?1:0)<<','<<stats.world_items<<','<<stats.awake_world_items<<','<<stats.item_assets<<','<<phase<<'\n';
    if(live_)output_.flush();
  }

private:
  diagnostics::AsyncProfileStream output_;
  std::int64_t last_cpu_time_{-1};
  bool live_=std::getenv("OCTARYN_CLIENT_LIVE_FRAME_TIMING")!=nullptr;
};
}
