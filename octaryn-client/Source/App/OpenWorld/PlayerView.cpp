#include "PlayerView.h"
#include "CameraBoom.h"
namespace octaryn::client::app {
rendering::WorldCamera player_camera_map(const LocalPlayerPose& pose,const WorldControls& controls,float fov) {
  const rendering::WorldCamera result{pose.x,pose.y,pose.z,controls.yaw,controls.pitch,fov};
  if(!controls.third_person)return result;
  // No voxel world means no occupancy query; the boom never retracts.
  return shoulder_camera(result,controls.shoulder,[](int,int,int){return false;});
}
}
