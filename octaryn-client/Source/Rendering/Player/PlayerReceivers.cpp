#include "PlayerRendererInternal.h"
#include "WorldRendererInternal.h"
#include "../BlockTransportGI/ReceiverGeometry.h"
#include <cmath>
#include <vector>
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
  auto& r=*renderer;
  if(world.gi_mode!=GiMode::BlockTransport || !pose.visible)return prepare_dynamic_receivers(r.indirect,world,commands,{});
  if(r.model.vertices.size()>DynamicReceiverCapacity/2 || !sample_player_frame(r,pose))return false;
  std::vector<DynamicReceiver> receivers(r.model.vertices.size()*2);
  const float cosine=std::cos(pose.yaw),sine=std::sin(pose.yaw);
  for(std::size_t index=0;index<r.model.vertices.size();++index) {
    const auto& vertex=r.model.vertices[index];std::array<ReceiverVector,4> columns{};
    for(unsigned influence=0;influence<4;++influence) {
      const auto joint=vertex.joints[influence];if(joint>=r.skin.size())return false;
      for(unsigned column=0;column<4;++column)for(unsigned axis=0;axis<3;++axis)
        columns[column][axis]+=r.skin[joint][column][axis]*vertex.weights[influence];
    }
    ReceiverGeometry geometry;
    if(!player_receiver_geometry(columns,{vertex.position[0],vertex.position[1],vertex.position[2]},
        {vertex.normal[0],vertex.normal[1],vertex.normal[2]},
        {pose.feet_x,pose.feet_y,pose.feet_z},cosine,sine,geometry))return false;
    for(unsigned side=0;side<2;++side) {
      auto& receiver=receivers[index*2+side];
      receiver.position={geometry.position[0],geometry.position[1],geometry.position[2],0};
      const float sign=side?-1.f:1.f;
      receiver.normal={geometry.normal[0]*sign,geometry.normal[1]*sign,geometry.normal[2]*sign,0};
      receiver.identity={static_cast<unsigned>(index*2+side),0x504c4159u,0,0};
    }
  }
  return prepare_dynamic_receivers(r.indirect,world,commands,receivers);
}
}
