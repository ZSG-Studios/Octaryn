#pragma once

#include "CharacterMotion.h"

namespace octaryn::character_motion {

bool move_walk_on_mesh(const Input &input, float dt, State &state,
                       float pitch, float yaw, const MeshCollision &mesh);

} // namespace octaryn::character_motion
