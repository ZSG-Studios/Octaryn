#include "PlayerModel.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace octaryn::client::rendering {
bool sample_player_local_pose(const PlayerModel& model,const std::string& name,double seconds,PlayerLocalPose& pose) {
  using namespace fastgltf::math;
  if(!std::isfinite(seconds) || model.nodes.size()>128 || model.joints.size()>PlayerMaxJoints) return false;
  pose.count=model.nodes.size();
  for(size_t i=0;i<pose.count;++i) pose.nodes[i]={model.nodes[i].translation,model.nodes[i].scale,model.nodes[i].rotation};
  if(!name.empty()) {
    const auto clip=std::find_if(model.animations.begin(),model.animations.end(),[&](const auto& a) {return a.name==name;});
    if(clip==model.animations.end()) return false;
    const float time=static_cast<float>(clip->looping?std::fmod(std::max(0.0,seconds),clip->duration):
        std::clamp(seconds,0.0,static_cast<double>(clip->duration)));
    for(const auto& track:clip->tracks) {
      const auto upper=std::upper_bound(track.times.begin(),track.times.end(),time);
      const size_t next=static_cast<size_t>(upper-track.times.begin());
      const size_t a=next==0?0:next-1,b=std::min(next,track.times.size()-1);
      const float t=track.step || a==b?0:std::clamp((time-track.times[a])/(track.times[b]-track.times[a]),0.0f,1.0f);
      const auto av=track.values[a],bv=track.values[b];auto& node=pose.nodes[track.node];
      if(track.path==PlayerTrackPath::Rotation) node.rotation=slerp(fquat(av[0],av[1],av[2],av[3]),fquat(bv[0],bv[1],bv[2],bv[3]),t);
      else {
        const fvec3 value(av[0]+(bv[0]-av[0])*t,av[1]+(bv[1]-av[1])*t,av[2]+(bv[2]-av[2])*t);
        if(track.path==PlayerTrackPath::Translation) node.translation=value;else node.scale=value;
      }
    }
  }
  return true;
}
bool blend_player_local_pose(const PlayerLocalPose& from,const PlayerLocalPose& to,float alpha,PlayerLocalPose& result) {
  using namespace fastgltf::math;
  if(from.count!=to.count || from.count>result.nodes.size() || !std::isfinite(alpha)) return false;
  if(alpha<=0) {result=from;return true;}
  if(alpha>=1) {result=to;return true;}
  result.count=from.count;
  for(size_t i=0;i<result.count;++i) {
    const auto& a=from.nodes[i];const auto& b=to.nodes[i];auto& out=result.nodes[i];
    out.translation=a.translation+(b.translation-a.translation)*alpha;
    out.scale=a.scale+(b.scale-a.scale)*alpha;
    out.rotation=normalize(slerp(normalize(a.rotation),normalize(b.rotation),alpha));
  }
  return true;
}
bool build_player_skin(const PlayerModel& model,const PlayerLocalPose& pose,
    std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>& palette) {
  using namespace fastgltf::math;
  if(pose.count!=model.nodes.size() || pose.count>128 || model.joints.size()>PlayerMaxJoints) return false;
  std::vector<fmat4x4> globals(pose.count);std::vector<bool> ready(pose.count);
  std::function<const fmat4x4&(size_t)> global=[&](size_t index)->const fmat4x4& {
    if(!ready[index]) {
      const auto& node=pose.nodes[index];
      const auto local=scale(rotate(translate(fmat4x4(),node.translation),node.rotation),node.scale);
      const auto parent=model.nodes[index].parent;
      globals[index]=parent<0?local:global(static_cast<size_t>(parent))*local;
      ready[index]=true;
    }
    return globals[index];
  };
  for(size_t i=0;i<model.joints.size();++i) palette[i]=global(model.joints[i])*model.inverse_bind[i];
  return true;
}
bool sample_player_skin(const PlayerModel& model,const std::string& name,double seconds,
    std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>& palette) {
  PlayerLocalPose pose;
  return sample_player_local_pose(model,name,seconds,pose) && build_player_skin(model,pose,palette);
}
bool PlayerAnimator::current_pose(const PlayerModel& model,double source_seconds,PlayerLocalPose& pose) const {
  if(!sample_player_local_pose(model,clip_,source_seconds-clip_start_,pose)) return false;
  if(!blending_) return true;
  const float elapsed=static_cast<float>(std::clamp((source_seconds-blend_start_)/.12,0.0,1.0));
  const float weight=elapsed*elapsed*(3-2*elapsed);
  return blend_player_local_pose(blend_from_,pose,weight,pose);
}
bool PlayerAnimator::sample(const PlayerModel& model,const std::string& clip,uint64_t action_sequence,
    double source_seconds,std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>& palette) {
  if(!std::isfinite(source_seconds)) return false;
  if(!initialized_ || source_seconds<previous_time_) {
    clip_start_=source_seconds;clip_=clip;sequence_=action_sequence;
    blending_=false;initialized_=true;
  } else if(clip_!=clip || sequence_!=action_sequence) {
    // Sample the outgoing local pose at this exact source time, including any
    // interrupted transition, before resetting a one-shot or movement clip.
    PlayerLocalPose outgoing;
    if(!current_pose(model,source_seconds,outgoing)) return false;
    blend_from_=outgoing;
    clip_start_=blend_start_=source_seconds;clip_=clip;sequence_=action_sequence;blending_=true;
  }
  previous_time_=source_seconds;
  PlayerLocalPose pose;
  if(!current_pose(model,source_seconds,pose)) return false;
  if(source_seconds-blend_start_>=.12) blending_=false;
  return build_player_skin(model,pose,palette);
}
}
