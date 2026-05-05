#include "PlayerSimulation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {

constexpr uint32_t WalkMode = 0u;
constexpr double PlayerPersistIntervalSeconds = 1.0;
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

double accumulate_save_seconds(double previous, double delta_seconds) {
  double accumulated =
      std::isfinite(previous) && previous > 0.0 ? previous : 0.0;
  if (std::isfinite(delta_seconds) && delta_seconds > 0.0) {
    accumulated += delta_seconds;
  }
  return accumulated;
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

uint32_t octaryn_server_player_should_save_state(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current, double seconds_since_last_save,
    uint32_t force) {
  if (octaryn_server_player_save_state_changed(previous, current) == 0u) {
    return 0u;
  }

  if (force != 0u) {
    return 1u;
  }

  return std::isfinite(seconds_since_last_save) &&
                 seconds_since_last_save >= PlayerPersistIntervalSeconds
             ? 1u
             : 0u;
}

int octaryn_server_player_save_decision(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current, double seconds_since_last_save,
    double delta_seconds, uint32_t force,
    OctarynServerPlayerSaveDecision *decision) {
  if (!decision) {
    return -1;
  }

  const double accumulated =
      accumulate_save_seconds(seconds_since_last_save, delta_seconds);
  const uint32_t should_save = octaryn_server_player_should_save_state(
      previous, current, accumulated, force);
  decision->should_save = should_save;
  decision->reserved = 0u;
  decision->seconds_since_last_save = should_save != 0u ? 0.0 : accumulated;
  return 0;
}
}
