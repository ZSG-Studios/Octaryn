#pragma once

#include "CharacterMotion.h"

namespace octaryn::character_motion {

bool move_walk_on_mesh(const Input &input, float dt, State &state,
                       float pitch, float yaw, const MeshCollision &mesh);

// Drop the cached Jolt shape before the backing map soup is destroyed. The
// cache is keyed by soup addresses for the hot path; explicit retirement
// prevents a later map allocation from reusing an address with stale geometry.
void release_mesh_collision(const MeshCollision &mesh);

} // namespace octaryn::character_motion
