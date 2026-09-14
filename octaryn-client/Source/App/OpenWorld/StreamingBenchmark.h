#pragma once
#include "FrameProfile.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace octaryn::client::app {
// Explicit camera/stream fixture: the authoritative player remains untouched.
class StreamingBenchmark {
public:
  explicit StreamingBenchmark(double speed):speed_(speed) {
    if(speed_<=0)return;
    const char* path=SDL_getenv("OCTARYN_CLIENT_STREAM_PROFILE_PATH");
    if(!path || !*path)throw std::runtime_error("Streaming benchmark requires OCTARYN_CLIENT_STREAM_PROFILE_PATH");
    const auto output=std::filesystem::path(reinterpret_cast<const char8_t*>(path));
#ifdef _WIN32
    if(_wfopen_s(&file_,output.c_str(),L"w")!=0)file_=nullptr;
#else
    file_=std::fopen(output.c_str(),"w");
#endif
    if(!file_)throw std::runtime_error("Cannot open streaming frame profile");
    std::fputs("frame,time_seconds,phase,frame_ms,session_ms,stream_mesh_ms,render_ms,ui_ms,events_ms,profile_ms,camera_x,camera_y,camera_z,center_x,center_z,columns,expected_columns,pending_meshes,quads,drawn_columns,drawn_quads,gpu_bytes,loading,render_width,render_height,temporal_resets\n",file_);
    std::printf("world_stream_benchmark fixture=camera_linear speed_mps=%.3f direction=negative_z altitude_offset=48 authority=unchanged\n",speed_);
  }
  ~StreamingBenchmark(){if(file_)std::fclose(file_);}
  void camera(LocalPlayerPose& view,double measured_seconds) {
    if(!file_)return;
    if(!ready_){origin_=view;origin_.y+=48;ready_=true;}
    view.x=origin_.x;view.y=origin_.y;
    view.z=origin_.z-static_cast<float>(speed_*std::max(0.0,measured_seconds));
  }
  void frame(const frame_profile_sample& sample,const rendering::WorldCamera& camera,
      const rendering::WorldRendererStats& stats,unsigned radius,double seconds,const char* phase) {
    if(!file_ || !ready_)return;
    const unsigned expected=(2*radius+1)*(2*radius+1);
    if(stats.columns>expected || stats.drawn_columns>stats.columns || stats.drawn_quads>stats.quads)
      throw std::runtime_error("Streaming benchmark renderer residency/geometry bounds exceeded");
    std::fprintf(file_,"%llu,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%u,%u,%u,%llu,%u,%llu,%llu,%u,%u,%u,%llu\n",
        static_cast<unsigned long long>(stats.frames),seconds,phase,
        sample.total_ms,sample.sim_ms,sample.world_ms,sample.render_ms,sample.ui_ms,sample.misc_ms,sample.post_submit_tail_ms,
        camera.x,camera.y,camera.z,int(std::floor(camera.x/32)),int(std::floor(camera.z/32)),
        stats.columns,expected,stats.pending_meshes,static_cast<unsigned long long>(stats.quads),stats.drawn_columns,
        static_cast<unsigned long long>(stats.drawn_quads),static_cast<unsigned long long>(stats.gpu_bytes),
        stats.columns<expected || stats.pending_meshes>0?1u:0u,stats.render_width,stats.render_height,
        static_cast<unsigned long long>(stats.temporal_resets));
  }
private:
  double speed_{};
  FILE* file_{};
  bool ready_{};
  LocalPlayerPose origin_{};
};
}
