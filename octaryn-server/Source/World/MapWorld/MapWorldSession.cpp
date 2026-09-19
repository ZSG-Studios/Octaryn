#include "MapWorld.h"

#include "MapWorldSession.h"
#include "CharacterMotion.h"

#include <bit>
#include <cmath>

namespace {

constexpr uint32_t WalkMode = 0u;
constexpr float MaxIntegratedDeltaSeconds = 0.25f;

// Mirrors octaryn_server_player_step's delta clamping.
float clamp_delta_seconds(double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 0.0f;
  }
  if (value > static_cast<double>(MaxIntegratedDeltaSeconds)) {
    return MaxIntegratedDeltaSeconds;
  }
  return static_cast<float>(value);
}

// Mirrors octaryn_server_player_has_input_intent; the map world keeps its
// player-simulation link surface to fastgltf/character_motion/glaze only.
uint32_t has_input_intent(const OctarynServerPlayerInput *input) {
  if (input == nullptr) {
    return 0u;
  }
  return input->controller != 0u || input->flags != 0u ||
                 input->move_x != 0.0f || input->move_y != 0.0f ||
                 input->move_z != 0.0f || input->relative_mouse != 0
             ? 1u
             : 0u;
}

} // namespace

extern "C" {

int octaryn_server_map_world_spawn(void *handle,
                                   OctarynServerPlayerState *state) {
  auto *world =
      static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
  if (world == nullptr || state == nullptr) {
    return -1;
  }

  state->x = world->manifest.spawn_x;
  state->y = world->manifest.spawn_y;
  state->z = world->manifest.spawn_z;
  state->pitch = world->manifest.pitch;
  state->yaw = world->manifest.yaw;
  state->velocity_x = 0.0f;
  state->velocity_y = 0.0f;
  state->velocity_z = 0.0f;
  state->is_on_ground = 0u;
  state->control_mode = WalkMode;
  state->jump_held = 0u;
  return 0;
}

int octaryn_server_map_world_step(void *handle,
                                  const OctarynServerPlayerInput *input,
                                  double delta_seconds,
                                  OctarynServerPlayerState *state,
                                  OctarynServerPlayerTickResult *result) {
  auto *world =
      static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
  if (world == nullptr || input == nullptr || state == nullptr ||
      result == nullptr) {
    return -1;
  }

  const float previous_x = state->x;
  const float previous_y = state->y;
  const float previous_z = state->z;
  result->tick_input = has_input_intent(input);
  result->reserved = 0u;
  result->delta_x = 0.0f;
  result->delta_y = 0.0f;
  result->delta_z = 0.0f;
  if (result->tick_input == 0u) {
    state->velocity_x = 0.0f;
    state->velocity_y = 0.0f;
    state->velocity_z = 0.0f;
    return 0;
  }

  const auto motion_input = std::bit_cast<octaryn::character_motion::Input>(*input);
  auto motion_state = std::bit_cast<octaryn::character_motion::State>(*state);
  const octaryn::character_motion::MeshCollision mesh{
      world->soup.positions.data(),
      world->soup.positions.size(),
      world->soup.indices.data(),
      world->soup.indices.size()};
  octaryn::character_motion::step_on_mesh(
      motion_input, clamp_delta_seconds(delta_seconds), motion_state, mesh);
  *state = std::bit_cast<OctarynServerPlayerState>(motion_state);

  result->delta_x = state->x - previous_x;
  result->delta_y = state->y - previous_y;
  result->delta_z = state->z - previous_z;
  return 0;
}

} // extern "C"
