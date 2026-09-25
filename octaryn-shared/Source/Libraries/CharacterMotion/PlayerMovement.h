#pragma once

#include "CharacterMotion.h"

namespace octaryn::character_motion {

void move_fly(const Input &input, float dt,
 State &state, float pitch, float yaw);

} // namespace octaryn::character_motion
