#pragma once
#include "SceneTransitionHost.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <glaze/glaze.hpp>
#include <vector>

namespace octaryn::client::app {
struct SceneTransitionRouteStep {std::string scene;std::array<double,5> camera{};unsigned hold_frames{60};};
struct SceneTransitionRouteDefinition {unsigned version{};std::vector<SceneTransitionRouteStep> steps;};
// Explicit hidden fixture camera and Use edges, without operating-system input.
// Exercises game-owned door selection/preparation, not movement or retail picking.
class SceneTransitionRoute {
  SceneTransitionRouteDefinition definition_;
  std::size_t step_{};
  unsigned frames_{};
  bool used_{},done_{};
public:
  SceneTransitionRoute(const char* path,bool hidden,bool bounded) {
    if(!path || !*path)return;
    if(!hidden || !bounded)throw std::runtime_error("Scene transition route requires bounded hidden qualification");
    const auto file=std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    const auto size=std::filesystem::file_size(file);
    if(!size || size>65536)throw std::runtime_error("Scene transition route byte admission");
    std::ifstream input(file,std::ios::binary);std::string text(size,'\0');
    if(!input.read(text.data(),static_cast<std::streamsize>(size)) || glz::read_json(definition_,text) ||
        definition_.version!=1 || definition_.steps.size()<2 || definition_.steps.size()>32)
      throw std::runtime_error("Scene transition route schema invalid");
    for(const auto& step:definition_.steps) {
      const auto& p=step.camera;
      if(step.hold_frames<60 || step.hold_frames>1200 || step.scene.empty() || step.scene.size()>255 || step.scene.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos ||
          !host::transition_pose_valid({p[0],p[1],p[2],float(p[3]),float(p[4])}))
        throw std::runtime_error("Scene transition route step invalid");
    }
    std::puts("scene_transition_route enabled=1 hidden=1 gameplay_movement=0");
  }
  bool active() const {return !definition_.steps.empty();}
  bool done() const {return done_;}
  bool menu_edge() const {return active() && step_==0 && frames_==2;}
  bool input(const std::string& scene,bool ready) {
    if(!active() || !ready || done_)return false;
    if(scene!=definition_.steps[step_].scene) {
      if(step_+1>=definition_.steps.size() || scene!=definition_.steps[step_+1].scene)
        throw std::runtime_error("Scene transition route reached unexpected scene");
      ++step_;frames_=0;used_=false;
      std::printf("scene_transition_route arrived=%zu asset=%s\n",step_,scene.c_str());
    }
    ++frames_;
    if(step_+1==definition_.steps.size()) {
      if(frames_>=definition_.steps[step_].hold_frames) {done_=true;std::printf("scene_transition_route passed=1 replacements=%zu\n",step_);}
      return false;
    }
    if(frames_>=definition_.steps[step_].hold_frames && !used_) {used_=true;return true;}
    return false;
  }
  template<class Camera> void camera(Camera& camera,const std::string& scene) const {
    if(!active())return;
    auto index=step_;
    if(scene!=definition_.steps[index].scene && index+1<definition_.steps.size() && scene==definition_.steps[index+1].scene)++index;
    const auto& p=definition_.steps[index].camera;
    camera.x=p[0];camera.y=p[1];camera.z=p[2];camera.yaw=float(p[3]);camera.pitch=float(p[4]);
  }
};
}
