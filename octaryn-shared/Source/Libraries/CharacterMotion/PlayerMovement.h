#pragma once

#include "CharacterMotion.h"

namespace octaryn::character_motion {

void move_fly(const Input &input, float dt,
 State &state, float pitch, float yaw);

void move_walk(const Input &input, float dt,
 State &state, float pitch, float yaw,
 SolidQuery block_query, void *context);

} // namespace octaryn::character_motion
