#pragma once

#include <cstdint>

namespace octaryn::character_motion {

struct Input {
 uint32_t flags;
 uint32_t controller;
 float move_x, move_y, move_z;
 float camera_x, camera_y, camera_z;
 float camera_pitch, camera_yaw;
 int32_t relative_mouse;
};

struct State {
 float x, y, z;
 float pitch, yaw;
 float velocity_x, velocity_y, velocity_z;
 uint32_t is_on_ground;
 uint32_t control_mode;
 uint16_t selected_block;
 uint16_t jump_held;
};

// Packed block query: low 16 bits are the block ID; bit 16 marks solidity.
using SolidQuery = uint32_t (*)(void *, int32_t, int32_t, int32_t);

void step(const Input &input, float deltaSeconds, State &state,
 SolidQuery query, void *context);

} // namespace octaryn::character_motion
