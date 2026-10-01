#include "PlayerSimulation.h"

#include <cmath>
#include <cstdint>

namespace {

constexpr uint32_t WalkMode = 0u;
constexpr uint32_t FlyMode = 1u;

constexpr float DefaultSpawnY = 80.0f;
constexpr float DefaultSpawnPitch = -0.35f;
constexpr float SpawnEyeHeight = 2.72f;

} // namespace

extern "C" {

float octaryn_server_player_spawn_eye_height() { return SpawnEyeHeight; }

const char *octaryn_server_player_control_mode_name(uint32_t mode) {
  return mode == FlyMode ? "fly" : "walk";
}

uint32_t octaryn_server_player_control_mode_is_fly(uint32_t mode) {
  return mode == FlyMode ? 1u : 0u;
}

int octaryn_server_player_default_state(OctarynServerPlayerState *state) {
  if (!state) {
    return -1;
  }

  state->x = 0.0f;
  state->y = DefaultSpawnY;
  state->z = 0.0f;
  state->pitch = DefaultSpawnPitch;
  state->yaw = 0.0f;
  state->velocity_x = 0.0f;
  state->velocity_y = 0.0f;
  state->velocity_z = 0.0f;
  state->is_on_ground = 0u;
  state->control_mode = WalkMode;
  state->jump_held = 0u;
  return 0;
}
}
