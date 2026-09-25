#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace octaryn::client::app {
// Camera-only fixture: no controls, player pose, or authority state is changed.
class MapMotionValidation {
public:
  MapMotionValidation(bool hidden, double seconds, int frame_limit,
      const char* requested, const char* path)
      : enabled_(hidden && seconds > 0 && frame_limit > 0 && requested &&
          std::strcmp(requested, "1") == 0) {
    if(!enabled_)return;
    if(!path || !*path)throw std::runtime_error("Map camera motion requires an evidence path");
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
    if(!output_)throw std::runtime_error("Cannot open map camera motion evidence");
    output_<<std::setprecision(9)<<"frame,ready_frame,phase,eye_x,eye_y,eye_z,yaw,pitch\n";
    std::printf("map_camera_motion fixture=translation_rotation_settle_cut hidden=1 bounded=1\n");
    std::fflush(stdout);
  }

  template<class Camera> void apply(Camera& camera, unsigned ready_frame) {
    if(!enabled_ || ready_frame < 180)return;
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

  template<class Camera> void record(unsigned long long frame, unsigned ready_frame,
      const Camera& camera) {
    if(!enabled_)return;
    const char* phase=ready_frame<180 ? "warmup" : ready_frame<276 ? "motion" :
        ready_frame<324 ? "settle" : ready_frame==324 ? "cut" : "post_cut";
    output_<<frame<<','<<ready_frame<<','<<phase<<','<<camera.x<<','<<camera.y<<','
        <<camera.z<<','<<camera.yaw<<','<<camera.pitch<<'\n';
    output_.flush();
  }

private:
  bool enabled_=false,origin_valid_=false;
  double x_=0,y_=0,z_=0;
  float yaw_=0,pitch_=0;
  std::ofstream output_;
};
}
