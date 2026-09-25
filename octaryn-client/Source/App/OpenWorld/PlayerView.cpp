#include "PlayerView.h"
#include "CameraBoom.h"

#include <cmath>

namespace octaryn::client::app {

rendering::WorldCamera player_camera_map(const MapPlayer& pose,const WorldControls& controls,float fov) {
  const rendering::WorldCamera result{pose.x,pose.y,pose.z,controls.yaw,controls.pitch,fov};
  if(!controls.third_person)return result;
  // The mesh map has no occupancy query; the boom never retracts.
  return shoulder_camera(result,controls.shoulder,[](int,int,int){return false;});
}

rendering::PlayerPose player_presentation(const MapPlayer& pose,const WorldControls& controls,
 const rendering::WorldCamera& camera,double presentation_seconds,double attack_until,uint64_t attack_sequence) {
  using Clip=rendering::PlayerClip;
  rendering::PlayerPose result;
  result.feet_x=pose.x;result.feet_y=pose.y-1.62f;result.feet_z=pose.z;
  result.yaw=controls.yaw;result.source_seconds=presentation_seconds;
  const float speed=std::hypot(pose.velocity_x,pose.velocity_z);
  result.clip=speed>.2f?(speed>18.0f?Clip::Run:Clip::Walk):Clip::Idle;
  if(!pose.flying) result.clip=pose.velocity_y>0?Clip::Jump:Clip::Fall;
  if(presentation_seconds<attack_until) {result.clip=Clip::Attack;result.action_sequence=attack_sequence;}
  const float dx=camera.x-pose.x,dy=camera.y-pose.y,dz=camera.z-pose.z;
  result.first_person=dx*dx+dy*dy+dz*dz<.36f;
  return result;
}

}
