#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include "../../Diagnostics/AsyncProfileStream.h"

namespace octaryn::client::app {
// Camera-only fixture: no controls, player pose, or authority state is changed.
class MapMotionValidation {
public:
  MapMotionValidation(bool hidden, double seconds, int frame_limit,
      const char* requested, const char* path)
      : enabled_(hidden && (seconds > 0 || frame_limit > 0) && requested &&
          (std::strcmp(requested, "1") == 0 || std::strcmp(requested,"static")==0)),
        moving_(requested && std::strcmp(requested,"1")==0) {
    if(!enabled_)return;
    if(const auto* origin=std::getenv("OCTARYN_CLIENT_MAP_CAMERA_ORIGIN")) {
      if(std::sscanf(origin,"%lf,%lf,%lf,%f,%f",&x_,&y_,&z_,&yaw_,&pitch_)!=5 ||
          !std::isfinite(x_) || !std::isfinite(y_) || !std::isfinite(z_) || !std::isfinite(yaw_) || !std::isfinite(pitch_))
        throw std::runtime_error("Invalid benchmark camera origin");
      origin_valid_=true;locked_=true;
    }
    if(!path || !*path)throw std::runtime_error("Map camera motion requires an evidence path");
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open map camera motion evidence");
    output_<<std::setprecision(9)<<"frame,ready_frame,phase,eye_x,eye_y,eye_z,yaw,pitch\n";
    std::printf("map_camera_motion fixture=translation_rotation_settle_cut hidden=1 bounded=1\n");
    std::fflush(stdout);
  }

  template<class Camera> void apply(Camera& camera, unsigned ready_frame) {
    if(!enabled_)return;
    if(locked_) {camera.x=x_;camera.y=y_;camera.z=z_;camera.yaw=yaw_;camera.pitch=pitch_;}
    if(!moving_ || ready_frame<180)return;
    if(!origin_valid_) {
      x_=camera.x; y_=camera.y; z_=camera.z;
      yaw_=camera.yaw; pitch_=camera.pitch;
      origin_valid_=true;
    }
    const float t=static_cast<float>(ready_frame-180)/96.0f;
    const float progress=std::fmin(t,1.0f);
    // Bounded sub-metre translation plus a look sweep exposes disocclusions.
    const float travel=0.65f*progress;
    camera.x=x_+std::cos(yaw_)*travel;
    camera.y=y_;
    camera.z=z_+std::sin(yaw_)*travel;
    camera.yaw=yaw_+0.24f*std::sin(progress*3.14159265359f);
    camera.pitch=pitch_+0.035f*std::sin(progress*3.14159265359f);
    if(ready_frame>=324) {
      camera.x=x_-0.35f*std::cos(yaw_);
      camera.z=z_-0.35f*std::sin(yaw_);
      camera.yaw=yaw_-0.30f;
      camera.pitch=pitch_;
    }
  }

  bool locked_scene() const {return enabled_ && locked_;}

  template<class Camera> void record(unsigned long long frame, unsigned ready_frame,
      const Camera& camera) {
    if(!enabled_)return;
    const char* phase=!moving_?"static":ready_frame<180 ? "warmup" : ready_frame<276 ? "motion" :
        ready_frame<324 ? "settle" : ready_frame==324 ? "cut" : "post_cut";
    output_<<frame<<','<<ready_frame<<','<<phase<<','<<camera.x<<','<<camera.y<<','
        <<camera.z<<','<<camera.yaw<<','<<camera.pitch<<'\n';
    output_.flush();
  }

private:
  bool enabled_=false,origin_valid_=false,moving_=false,locked_=false;
  double x_=0,y_=0,z_=0;
  float yaw_=0,pitch_=0;
  diagnostics::AsyncProfileStream output_;
};
}
