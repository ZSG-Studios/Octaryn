#pragma once
#include "../LocalSession/LocalSession.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glaze/glaze.hpp>
#include <optional>
#include <stdexcept>
#include <vector>

namespace octaryn::client::app {
struct ScenePhysicsRouteStep {std::string action;unsigned hold_frames{120};std::array<double,3> aim_offset{};};
struct ScenePhysicsRouteDefinition {unsigned version{};std::string scene;uint64_t source_id{};std::array<double,3> local_point{};std::vector<ScenePhysicsRouteStep> steps;};
struct ScenePhysicsRouteBody {uint64_t SourceId{};double X{},Y{},Z{},RotationX{},RotationY{},RotationZ{},RotationW{1};unsigned Flags{};};
struct ScenePhysicsRouteTarget {uint64_t SourceId{};};
struct ScenePhysicsRouteSnapshot {std::vector<ScenePhysicsRouteBody> Bodies;std::optional<ScenePhysicsRouteTarget> Target;};
// Explicit hidden virtual Z/E edges reach the game module and authority journal.
// Camera aiming follows an authored collision point in the replicated body pose.
class ScenePhysicsRoute {
  ScenePhysicsRouteDefinition definition_;
  ScenePhysicsRouteBody body_;
  std::size_t step_{};
  unsigned frames_{};
  bool done_{},have_body_{},sent_{};
  std::array<double,3> grab_start_{};
public:
  ScenePhysicsRoute(const char* path,bool hidden,bool bounded) {
    if(!path || !*path)return;
    if(!hidden || !bounded)throw std::runtime_error("Scene physics route requires bounded hidden qualification");
    auto file=std::filesystem::u8path(path);const auto size=std::filesystem::file_size(file);
    if(!size || size>65536)throw std::runtime_error("Scene physics route byte admission");
    std::ifstream input(file,std::ios::binary);std::string text(size,'\0');
    if(!input.read(text.data(),std::streamsize(size)) || glz::read_json(definition_,text) ||
       definition_.version!=1 || !definition_.source_id || definition_.scene.empty() ||
       definition_.steps.empty() || definition_.steps.size()>16)
      throw std::runtime_error("Scene physics route schema invalid");
    for(double v:definition_.local_point)if(!std::isfinite(v) || std::abs(v)>100)throw std::runtime_error("Scene physics route point invalid");
    for(const auto& s:definition_.steps) {
      if(s.hold_frames<30 || s.hold_frames>1200 || (s.action!="grab" && s.action!="release" && s.action!="pickup" && s.action!="finish"))
        throw std::runtime_error("Scene physics route step invalid");
      for(double v:s.aim_offset)if(!std::isfinite(v) || std::abs(v)>1)throw std::runtime_error("Scene physics route aim offset invalid");
    }
    std::printf("scene_physics_route enabled=1 source=%llu hidden=1\n",(unsigned long long)definition_.source_id);
  }
  bool active()const{return !definition_.steps.empty();}
  bool done()const{return done_;}
  bool menu_edge()const{return false;}
  unsigned input(const LocalSession& session,const std::string& scene,bool ready) {
    if(!active() || !ready || done_)return 0;
    if(scene!=definition_.scene)throw std::runtime_error("Scene physics route unexpected scene");
    std::string text;ScenePhysicsRouteSnapshot snapshot;
    constexpr auto options=glz::opts{.error_on_unknown_keys=false};
    if(!session.scene_physics_snapshot(text) || glz::read<options>(snapshot,text))return 0;
    for(const auto& b:snapshot.Bodies)if(b.SourceId==definition_.source_id){body_=b;have_body_=true;break;}
    if(!have_body_)return 0;
    ++frames_;
    const auto& s=definition_.steps[step_];
    if(sent_) {
      const bool accepted=s.action=="grab"?(body_.Flags&2)!=0:s.action=="release"?(body_.Flags&2)==0:(body_.Flags&4)!=0;
      if(!accepted) {
        if(frames_>s.hold_frames+180)throw std::runtime_error("Scene physics route action lacked authoritative confirmation");
        return 0;
      }
      std::printf("scene_physics_route confirmed=%s source=%llu flags=%u\n",s.action.c_str(),(unsigned long long)body_.SourceId,body_.Flags);
      ++step_;frames_=0;sent_=false;
      if(step_==definition_.steps.size()){done_=true;std::puts("scene_physics_route passed=1");}
      return 0;
    }
    if(frames_<s.hold_frames)return 0;
    if(s.action=="finish"){done_=true;std::puts("scene_physics_route passed=1");return 0;}
    if(s.action!="release" && (!snapshot.Target || snapshot.Target->SourceId!=definition_.source_id)) {
      if(frames_>s.hold_frames+180)throw std::runtime_error("Scene physics route source did not become HUD target");
      return 0;
    }
    if(s.action=="grab")grab_start_={body_.X,body_.Y,body_.Z};
    if(s.action=="release") {
      const double distance=std::hypot(body_.X-grab_start_[0],body_.Y-grab_start_[1],body_.Z-grab_start_[2]);
      std::printf("scene_physics_route movement_m=%.6f source=%llu\n",distance,(unsigned long long)body_.SourceId);
      if(distance<.02)throw std::runtime_error("Scene physics route camera grab did not move the native body");
    }
    sent_=true;std::printf("scene_physics_route edge=%s source=%llu\n",s.action.c_str(),(unsigned long long)body_.SourceId);
    return s.action=="pickup"?64u:128u;
  }
  template<class Camera> void camera(Camera& camera,const std::string& scene)const {
    if(!active() || !have_body_ || done_ || scene!=definition_.scene)return;
    const auto& p=definition_.local_point;const auto& b=body_;const auto& o=definition_.steps[step_].aim_offset;
    const double tx=2*(b.RotationY*p[2]-b.RotationZ*p[1]),ty=2*(b.RotationZ*p[0]-b.RotationX*p[2]),tz=2*(b.RotationX*p[1]-b.RotationY*p[0]);
    const double dx=b.X+p[0]+b.RotationW*tx+b.RotationY*tz-b.RotationZ*ty+o[0]-camera.x;
    const double dy=b.Y+p[1]+b.RotationW*ty+b.RotationZ*tx-b.RotationX*tz+o[1]-camera.y;
    const double dz=b.Z+p[2]+b.RotationW*tz+b.RotationX*ty-b.RotationY*tx+o[2]-camera.z;
    camera.yaw=float(std::atan2(dx,-dz));camera.pitch=float(std::atan2(dy,std::hypot(dx,dz)));
  }
};
}
