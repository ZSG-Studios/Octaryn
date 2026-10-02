#pragma once

#include "CharacterMotion.h"
#include "MeshCollisionWorld.h"

namespace octaryn::character_motion {

// One walk substep against the cached Box3D map world. Dynamic props receive
// bounded contact impulses; world stepping remains with the authority owner.
bool move_walk_on_mesh(const Input &input, float dt, State &state,
                       float pitch, float yaw, MeshCollisionWorld *world);

} // namespace octaryn::character_motion
