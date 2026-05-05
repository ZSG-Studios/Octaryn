#include "PlayerSimulation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {

constexpr uint32_t WalkMode = 0u;
constexpr float PlayerPositionPersistEpsilon = 0.01f;
constexpr float PlayerAnglePersistEpsilon = 0.001f;
constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;

float clamp_pitch(float pitch) {
  return std::clamp(pitch, -Pi * 0.5f + FLT_EPSILON,
                    Pi * 0.5f - FLT_EPSILON);
}

float normalize_yaw(float yaw) {
  yaw = std::fmod(yaw + Pi, TwoPi);
  if (yaw < 0.0f) {
    yaw += TwoPi;
  }
  return yaw - Pi;
}

bool changed_beyond(float previous, float current, float epsilon) {
  return std::fabs(previous - current) > epsilon;
}

} // namespace

extern "C" {

int octaryn_server_player_state_from_save(float x, float y, float z,
                                          float pitch, float yaw,
                                          uint16_t selected_block,
                                          OctarynServerPlayerState *state) {
  if (!state) {
    return -1;
  }

  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      !std::isfinite(pitch) || !std::isfinite(yaw)) {
    return 1;
  }

  state->x = x;
  state->y = std::clamp(y, -1000.0f, 1000.0f);
  state->z = z;
  state->pitch = clamp_pitch(pitch);
  state->yaw = normalize_yaw(yaw);
  state->velocity_x = 0.0f;
  state->velocity_y = 0.0f;
  state->velocity_z = 0.0f;
  state->is_on_ground = 0u;
  state->control_mode = WalkMode;
  state->selected_block = selected_block;
  state->reserved = 0u;
  return 0;
}

uint32_t octaryn_server_player_save_state_changed(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current) {
  if (!previous || !current) {
    return 1u;
  }

  return changed_beyond(previous->x, current->x,
                        PlayerPositionPersistEpsilon) ||
                 changed_beyond(previous->y, current->y,
                                PlayerPositionPersistEpsilon) ||
                 changed_beyond(previous->z, current->z,
                                PlayerPositionPersistEpsilon) ||
                 changed_beyond(previous->pitch, current->pitch,
                                PlayerAnglePersistEpsilon) ||
                 changed_beyond(previous->yaw, current->yaw,
                                PlayerAnglePersistEpsilon) ||
                 previous->selected_block != current->selected_block
             ? 1u
             : 0u;
}
}
