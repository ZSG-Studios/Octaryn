#include "PlayerSimulation.h"

#include "CharacterGeometry.h"

namespace {

bool block_intersects_range(int32_t block_coordinate, float min, float max) {
  const float block_min = static_cast<float>(block_coordinate);
  const float block_max = block_min + 1.0f;
  return block_min < max && block_max > min;
}

} // namespace

extern "C" {

uint32_t octaryn_server_player_session_intersects_block(void *session,
                                                        int32_t block_x,
                                                        int32_t block_y,
                                                        int32_t block_z) {
  if (!session) {
    return 0u;
  }

  const auto *native_session =
      static_cast<const OctarynServerPlayerSession *>(session);
  const OctarynServerPlayerState &state = native_session->state;
  const float min_y =
 state.y - octaryn::character_motion::EyeOffset;
  const float max_y =
 min_y + octaryn::character_motion::CollisionHeight;
  return block_intersects_range(
             block_x,
 state.x - octaryn::character_motion::CollisionRadius,
 state.x + octaryn::character_motion::CollisionRadius) &&
                 block_intersects_range(block_y, min_y, max_y) &&
                 block_intersects_range(
                     block_z,
                     state.z -
 octaryn::character_motion::CollisionRadius,
                     state.z +
 octaryn::character_motion::CollisionRadius)
             ? 1u
             : 0u;
}

}
