#pragma once
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::app {
// Explicit authoring camera; never changes the authoritative player or gameplay scale.
class ScenePreviewCamera {
public:
  explicit ScenePreviewCamera(const char* requested) {
    if(!requested || !*requested)return;
    char extra{};
    if(std::sscanf(requested,"%lf,%lf,%lf,%f,%f%c",&x_,&y_,&z_,&yaw_,&pitch_,&extra)!=5 ||
       !valid(x_) || !valid(y_) || !valid(z_) || !valid(yaw_) || !valid(pitch_) || std::abs(pitch_)>1.55f)
      throw std::runtime_error("Invalid scene preview camera: expected bounded x,y,z,yaw,pitch");
    enabled_=true;
    std::printf("scene_preview_camera enabled=1 renderer_only=1 authority_movement_suppressed=1 origin=%.6f,%.6f,%.6f\n",x_,y_,z_);
  }
  bool enabled() const {return enabled_;}
  template<class Pose> void set_origin(const Pose& pose) {
    if(!enabled_)return;
    x_=pose.x;y_=pose.y;z_=pose.z;yaw_=pose.yaw;pitch_=pose.pitch;initialized_=false;
  }
  template<class Input> void suppress(Input& input) const {
    if(!enabled_)return;
    input.forward=input.backward=input.left=input.right=input.up=input.down=input.sprint=input.flying=false;
    input.jump_events={};input.has_jump_events=false;
  }
  template<class Camera,class Controls> void apply(Camera& camera,Controls& controls,double seconds) {
    if(!enabled_)return;
    if(!initialized_) {controls.yaw=yaw_;controls.pitch=pitch_;initialized_=true;}
    const auto& move=controls.movement;
    double forward=move.move_forward-move.move_backward;
    double strafe=move.move_right-move.move_left;
    double up=move.move_up-move.move_down;
    const double sine=std::sin(controls.yaw),cosine=std::cos(controls.yaw);
    const double pitchCosine=std::cos(controls.pitch),pitchSine=std::sin(controls.pitch);
    const double dx=pitchCosine*sine*forward+cosine*strafe;
    const double dy=pitchSine*forward+up;
    const double dz=-pitchCosine*cosine*forward+sine*strafe;
    const double length=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(length>0 && std::isfinite(seconds) && seconds>0) {
      const double distance=std::fmin(seconds,0.1)*(move.sprint?4.0:1.0)/length;
      x_+=dx*distance;y_+=dy*distance;z_+=dz*distance;
      x_=std::fmax(-1000000,std::fmin(x_,1000000));
      y_=std::fmax(-1000000,std::fmin(y_,1000000));
      z_=std::fmax(-1000000,std::fmin(z_,1000000));
    }
    camera.x=x_;camera.y=y_;camera.z=z_;camera.yaw=controls.yaw;camera.pitch=controls.pitch;
  }
private:
  static bool valid(double value) {return std::isfinite(value) && std::abs(value)<=1000000;}
  bool enabled_{},initialized_{};
  double x_{},y_{},z_{};
  float yaw_{},pitch_{};
};
}
