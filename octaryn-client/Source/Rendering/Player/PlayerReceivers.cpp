#include "PlayerRendererInternal.h"
#include "WorldRendererInternal.h"
#include "../RenderBackend/DynamicReceivers.h"
#include <cmath>
#include <bit>

namespace octaryn::client::rendering {
bool initialize_player_receivers(PlayerRenderer* renderer,WorldRenderer& world) {
  return !renderer || initialize_dynamic_receivers(renderer->indirect,world);
}
const DynamicReceivers* player_receiver_stats(const PlayerRenderer* renderer) {return renderer?&renderer->indirect:nullptr;}
std::uint64_t player_occlusion_signature(const PlayerRenderer* renderer,const PlayerPose& pose) {
  if(!renderer || !pose.visible)return 0;
  std::uint64_t signature=14695981039346656037ull;
  const auto append=[&](float value) {signature^=std::bit_cast<std::uint32_t>(value);signature*=1099511628211ull;};
  for(float value:{pose.feet_x,pose.feet_y,pose.feet_z,pose.yaw})append(value);
  for(std::size_t joint=0;joint<renderer->model.joints.size();++joint)
    for(unsigned column=0;column<4;++column)for(unsigned axis=0;axis<4;++axis)append(renderer->skin[joint][column][axis]);
  return signature;
}
bool prepare_player_receivers(PlayerRenderer* renderer,WorldRenderer& world,rhi::ICommandEncoder* commands,const PlayerPose& pose) {
  if(!renderer)return true;
  // Indirect receive returns with the new world geometry streaming; the
  // receiver probe path stays dormant with an empty upload.
  (void)pose;
  return prepare_dynamic_receivers(renderer->indirect,world,commands,{});
}
}
