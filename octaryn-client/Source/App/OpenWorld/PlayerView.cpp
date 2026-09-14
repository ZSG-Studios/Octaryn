#include "PlayerView.h"
#include "CameraBoom.h"
#include "WorldStream.h"
#include "BlockInteraction.h"
namespace octaryn::client::app {
rendering::WorldCamera player_camera(const LocalPlayerPose& pose,const WorldControls& controls,float fov,
    const world_presentation::WorldStream& world,const world_presentation::BlockInteraction& blocks) {
  rendering::WorldCamera result{pose.x,pose.y,pose.z,controls.yaw,controls.pitch,fov};
  if(!controls.third_person)return result;
  return shoulder_camera(result,controls.shoulder,[&](int x,int y,int z) {
    std::uint16_t block{};
    return !world.try_block(x,y,z,block) || blocks.blocks_camera(block);
  });
}
}
