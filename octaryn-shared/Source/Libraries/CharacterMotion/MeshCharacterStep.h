#pragma once

#include "CharacterMotion.h"
#include "MeshCollisionWorld.h"

namespace octaryn::character_motion {

// One walk substep against the cached Box3D map world. Deterministic: no
// dynamic bodies, no internal Box3D stepping, queries only.
bool move_walk_on_mesh(const Input &input, float dt, State &state,
                       float pitch, float yaw, MeshCollisionWorld *world);

} // namespace octaryn::character_motion
