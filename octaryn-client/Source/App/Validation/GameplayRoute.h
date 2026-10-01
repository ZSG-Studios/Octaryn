#pragma once
#include "GameplayRouteDefinition.h"
#include "../LocalSession/LocalSession.h"
#include "../../Diagnostics/AsyncProfileStream.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iomanip>

namespace octaryn::client::app {
// Qualification input goes through prediction, transport and server authority.
class GameplayRoute {
  GameplayRouteDefinition definition_;
  diagnostics::AsyncProfileStream output_;
  LocalPlayerPose last_{};
  LocalPlayerInput command_{};
  std::size_t phase_{};
  std::uint64_t last_ack_{};
  double time_{},phase_start_{},distance_{},phase_distance_{},window_start_{},window_distance_{};
  float start_yaw_{},start_pitch_{};
  bool active_{},legacy_{},started_{},have_pose_{},failed_{},blocked_{},finished_{};
  unsigned blocked_windows_{};
  double target_error(const GameplayRoutePhase& phase) const {
    return phase.target ? std::hypot(last_.x-(*phase.target)[0],last_.z-(*phase.target)[1]) : -1;
  }
  void finish_phase() {
    const auto& phase=definition_.phases[phase_];
    const bool passed=have_pose_ && phase_distance_+0.01>=phase.min_distance &&
        (!phase.target || target_error(phase)<=phase.tolerance);
    failed_|=!passed;
    std::printf("gameplay_route_phase name=%s completed=1 passed=%d authority_distance=%.3f target_error=%.3f blocked_windows=%u\n",
        phase.name.c_str(),passed?1:0,phase_distance_,target_error(phase),blocked_windows_);
  }
public:
  bool authored() const {return active_ && !legacy_;}
  GameplayRoute(bool hidden,double bounded_seconds,int frame_limit) {
    const char* path=std::getenv("OCTARYN_CLIENT_GAMEPLAY_ROUTE");
    const char* legacy=std::getenv("OCTARYN_CLIENT_SCRIPTED_MOVE");
    if((!path || !*path) && (!legacy || !*legacy))return;
    if(!hidden || (bounded_seconds<=0 && frame_limit<=0))
      throw std::runtime_error("Gameplay route requires hidden bounded qualification");
    if(path && *path) {
      definition_=load_gameplay_route(std::filesystem::path(reinterpret_cast<const char8_t*>(path)));
      const char* csv=std::getenv("OCTARYN_CLIENT_GAMEPLAY_ROUTE_PATH");
      if(!csv || !*csv)throw std::runtime_error("Gameplay route requires CSV evidence path");
      output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(csv)));
      if(!output_)throw std::runtime_error("Cannot open gameplay route evidence");
      output_<<std::setprecision(9)<<"frame,seconds,phase,phase_index,phase_seconds,authority_tick,ack,sent_input,authority_x,authority_y,authority_z,authority_yaw,authority_pitch,authority_distance,phase_distance,forward,strafe,sprint,command_yaw,command_pitch,target_error,blocked,pending\n";
    } else {
      char* end{};const double seconds=std::strtod(legacy,&end);
      if(!end || *end || !std::isfinite(seconds) || seconds<0 || seconds>86400)
        throw std::runtime_error("Invalid scripted movement duration");
      if(seconds==0)return;
      GameplayRoutePhase phase;phase.name="legacy_forward";phase.seconds=seconds;phase.forward=1;
      definition_.phases.push_back(phase);legacy_=true;
    }
    active_=true;
    std::printf("gameplay_route active=1 phases=%zu hidden=1 bounded=1 authority=normal_transport legacy=%d\n",
        definition_.phases.size(),legacy_?1:0);
  }

  bool apply(LocalPlayerInput& input,double elapsed,bool ready,const LocalSession& session) {
    if(!active_ || !ready)return false;
    LocalPlayerPose authority;std::uint64_t ack{};
    if(!session.authority_pose(authority,ack))return false;
    if(!started_) {
      started_=true;last_=authority;have_pose_=true;
      start_yaw_=authority.yaw;start_pitch_=authority.pitch;
    } else time_+=std::max(0.0,elapsed);
    while(phase_<definition_.phases.size() && time_-phase_start_>=definition_.phases[phase_].seconds) {
      finish_phase();phase_start_+=definition_.phases[phase_].seconds;++phase_;phase_distance_=0;
      window_start_=time_;window_distance_=distance_;blocked_=false;
      start_yaw_=command_.yaw;start_pitch_=command_.pitch;
    }
    if(legacy_) {
      if(phase_<definition_.phases.size())input.forward=true;
      command_=input;
      return false;
    }
    input={};input.has_jump_events=true;
    input.yaw=command_.yaw;input.pitch=command_.pitch;
    if(phase_>=definition_.phases.size())return true;
    const auto& phase=definition_.phases[phase_];
    input.forward=phase.forward>0;input.backward=phase.forward<0;
    input.right=phase.strafe>0;input.left=phase.strafe<0;input.sprint=phase.sprint;
    const auto progress=float(std::clamp((time_-phase_start_)/phase.seconds,0.0,1.0));
    input.yaw=phase.yaw.value_or(start_yaw_)+phase.turn*progress;
    input.pitch=phase.pitch.value_or(start_pitch_);
    if(phase.target) {
      const float dx=(*phase.target)[0]-authority.x,dz=(*phase.target)[1]-authority.z;
      if(std::hypot(dx,dz)<=phase.tolerance)input.forward=false;
      else input.yaw=std::atan2(dx,-dz);
    }
    command_=input;
    return true;
  }

  void record(unsigned frame,const LocalSession& session) {
    if(!active_ || !started_ || phase_>=definition_.phases.size())return;
    LocalPlayerPose authority;std::uint64_t ack{};
    if(!session.authority_pose(authority,ack))return;
    if(have_pose_ && (authority.source_tick<last_.source_tick || ack<last_ack_))
      throw std::runtime_error("Gameplay authority clock/ack regressed");
    if(have_pose_ && authority.source_tick!=last_.source_tick) {
      const auto travel=std::hypot(authority.x-last_.x,authority.z-last_.z);
      if(!std::isfinite(travel))throw std::runtime_error("Nonfinite authority route pose");
      distance_+=travel;phase_distance_+=travel;
    }
    last_=authority;last_ack_=ack;have_pose_=true;
    if(time_-window_start_>=1) {
      blocked_=(command_.forward || command_.backward || command_.left || command_.right) &&
          distance_-window_distance_<.1;
      if(blocked_)++blocked_windows_;
      window_distance_=distance_;window_start_=time_;
    }
    const auto& phase=definition_.phases[phase_];
    if(legacy_) {
      if(frame%60==0)std::printf("scripted_move eye=%.3f,%.3f,%.3f remaining=%.1f\n",
          authority.x,authority.y,authority.z,std::max(0.0,phase.seconds-time_));
      return;
    }
    output_<<frame<<','<<time_<<','<<phase.name<<','<<phase_<<','<<time_-phase_start_<<','<<authority.source_tick<<','<<ack<<','
      <<session.last_sent_input_frame()<<','<<authority.x<<','<<authority.y<<','<<authority.z<<','<<authority.yaw<<','
      <<authority.pitch<<','<<distance_<<','<<phase_distance_<<','<<int(command_.forward)-int(command_.backward)<<','
      <<int(command_.right)-int(command_.left)<<','<<command_.sprint<<','<<command_.yaw<<','<<command_.pitch<<','
      <<target_error(phase)<<','<<blocked_<<','<<session.movement_stats().pending<<'\n';
    output_.flush();
    if(!output_)throw std::runtime_error("Gameplay route evidence writer failed");
  }
  bool finish() {
    if(!active_ || finished_)return !failed_;
    finished_=true;
    const bool complete=phase_==definition_.phases.size();
    const bool passed=legacy_ || (complete && !failed_);
    std::printf("gameplay_route_result passed=%d complete=%d authority_distance=%.3f seconds=%.3f blocked_windows=%u\n",
        passed?1:0,complete?1:0,distance_,time_,blocked_windows_);
    return passed;
  }
};
}
