#pragma once

#include "CharacterMotion.h"

namespace octaryn::character_motion {

bool move_walk_with_jolt(const Input &input, float dt,
 State &state, float pitch,
                         float yaw,
 SolidQuery block_query,
                         void *context);

} // namespace octaryn::character_motion
