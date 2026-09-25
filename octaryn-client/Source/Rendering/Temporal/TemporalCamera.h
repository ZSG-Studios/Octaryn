#pragma once
#include "WorldRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace octaryn::client::rendering {
struct TemporalView {
  std::array<float,4> position{},right{},up{},forward{},projection{};
};
inline TemporalView temporal_view(const WorldCamera& eye,int width,int height) {
  const float sy=std::sin(eye.yaw),cy=std::cos(eye.yaw),sp=std::sin(eye.pitch),cp=std::cos(eye.pitch);
  const float focal=1/std::tan(std::clamp(eye.vertical_fov,.2f,2.7f)/2);
  return {{eye.x,eye.y,eye.z,0},{cy,0,sy,0},{-sy*sp,cp,cy*sp,0},{sy*cp,sp,-cy*cp,0},
      {focal*float(height)/float(width),focal,8192.f/8191.9f,819.2f/8191.9f}};
}
// History tracks successful submissions, independently of reusable frame slots.
class TemporalCamera {
  WorldCamera previous_{};
  int width_{},height_{};
  bool valid_{};
public:
  bool reset(const WorldCamera& eye,int width,int height,double seconds,bool dynamic=false) const {
    const double x=double(eye.x)-previous_.x,y=double(eye.y)-previous_.y,z=double(eye.z)-previous_.z;
    return !valid_ || (!dynamic && (width!=width_ || height!=height_)) || !std::isfinite(seconds) || seconds<=0 || seconds>.25 ||
        x*x+y*y+z*z>4096 || std::abs(std::remainder(eye.yaw-previous_.yaw,6.283185307f))>1 ||
        std::abs(eye.pitch-previous_.pitch)>1 || std::abs(eye.vertical_fov-previous_.vertical_fov)>.05f;
  }
  int previous_width() const {return width_;}
  int previous_height() const {return height_;}
  const WorldCamera& previous() const {return previous_;}
  void commit(const WorldCamera& eye,int width,int height) {previous_=eye;width_=width;height_=height;valid_=true;}
  void invalidate() {valid_=false;}
};
}
