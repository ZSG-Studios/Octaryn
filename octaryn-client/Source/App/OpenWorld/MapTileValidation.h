#pragma once
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include "../../Diagnostics/AsyncProfileStream.h"

namespace octaryn::client::app {
// Renderer-only camera route; never edits controls, player poses, or authority.
class MapTileValidation {
public:
  MapTileValidation(bool hidden,double seconds,int frame_limit,const char* requested,const char* path)
      : enabled_(hidden && (seconds>0 || frame_limit>0) && requested && std::strcmp(requested,"1")==0) {
    if(!enabled_)return;
    if(const char* warmup=std::getenv("OCTARYN_CLIENT_TILE_CAMERA_WARMUP_FRAMES"))
      warmup_=unsigned(std::clamp(std::atoi(warmup),180,10000));
    if(!path || !*path)throw std::runtime_error("Tile route requires an evidence path");
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open tile route evidence");
    output_<<std::setprecision(9)<<"frame,ready_frame,phase,cycle,eye_x,eye_y,eye_z,authority_x,authority_y,authority_z,route_elapsed_seconds\n";
    std::printf("tile_camera_route hidden=1 bounded=1 span_m=384 authority_modified=0 warmup_frames=%u\n",warmup_);
  }
  template<class Camera> void apply(Camera& camera,unsigned frame) {
    if(!enabled_ || frame<warmup_)return;
    if(!origin_) {x_=camera.x;y_=camera.y;z_=camera.z;origin_=true;}
    const unsigned step=(frame-warmup_)%960;
    float progress=0;
    if(step<4)progress=(step%2)==0?1.f:0.f;
    else if(step<120)progress=0;
    else if(step<360)progress=float(step-120)/240;
    else if(step<480)progress=1;
    else if(step<720)progress=1-float(step-480)/240;
    camera.x=x_+384*progress;camera.y=y_;camera.z=z_;
    camera.yaw=3.14159265f;camera.pitch=-.15f;
  }
  template<class Camera,class Pose> void record(unsigned long long frame,unsigned ready,
      const Camera& camera,const Pose& authority) {
    if(!enabled_)return;
    const unsigned step=ready<warmup_?0:(ready-warmup_)%960;
    const char* phase=ready<warmup_?"warmup":step<4?"cut":step<120?"origin":step<360?"outbound":
        step<480?"far":step<720?"inbound":"origin";
    const auto now=std::chrono::steady_clock::now();
    if(ready>=warmup_ && !route_started_){route_started_=true;route_start_=now;}
    elapsed_=route_started_?std::chrono::duration<double>(now-route_start_).count():0;
    output_<<frame<<','<<ready<<','<<phase<<','<<(ready<warmup_?0:(ready-warmup_)/960)<<','
        <<camera.x<<','<<camera.y<<','<<camera.z<<','<<authority.x<<','<<authority.y<<','<<authority.z<<','<<elapsed_<<'\n';
    if(ready%30==0)output_.flush();
  }
  bool enabled() const {return enabled_;}
  bool capture_ready(unsigned ready) const {
    if(!enabled_ || ready<warmup_)return true;
    const unsigned step=(ready-warmup_)%960;
    return (step>=4 && step<120) || step>=720;
  }
  bool duration_complete(double seconds) const {
    return route_started_ && elapsed_>=seconds;
  }
private:
  bool enabled_{},origin_{};
  bool route_started_{};
  unsigned warmup_{180};
  double elapsed_{};
  std::chrono::steady_clock::time_point route_start_{};
  double x_{},y_{},z_{};
  diagnostics::AsyncProfileStream output_;
};
}
