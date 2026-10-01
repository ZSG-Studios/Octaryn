#include "MapWorld.h"

#include "MapWorldSession.h"
#include "CharacterMotion.h"
#include "CharacterCollision.h"

#include <bit>
#include <cmath>
#include <algorithm>

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

int octaryn_server_map_world_collision_ready(void* handle, float x, float z, float radius) {
  auto* world = static_cast<octaryn::server::map_world::ServerMapWorld*>(handle);
  // This existing ABI has no height; scene catalogs require the explicit 3D export.
  if(!world || !world->manifest.scene_catalog.empty())return -1;
  return octaryn_server_map_world_collision_ready_at(handle,x,world->manifest.spawn_y,z,radius);
}

int octaryn_server_map_world_collision_ready_at(void* handle, float x, float y, float z, float radius) {
  auto* world = static_cast<octaryn::server::map_world::ServerMapWorld*>(handle);
  if (!world || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(radius) || radius < 0 || radius > 4096) return -1;
  const bool ready = world->ready(x, y, z, radius);
  if (world->tiles && world->tiles->stats().failed) return -2;
  return ready ? 0 : 1;
}

int octaryn_server_map_world_collision_ready_state(void* handle,const OctarynServerPlayerState* state,
    const OctarynServerPlayerInput* input,double delta_seconds) {
  if(!state || !input)return -1;
  const auto body=std::bit_cast<octaryn::character_motion::State>(*state);
  const auto command=std::bit_cast<octaryn::character_motion::Input>(*input);
  const auto radius=octaryn::character_motion::character_collision_radius(body,command,clamp_delta_seconds(delta_seconds));
  return octaryn_server_map_world_collision_ready_at(handle,body.x,body.y,body.z,radius);
}

int octaryn_server_map_world_collision_stats(void* handle, OctarynCollisionResidencyStats* stats, uint32_t byte_size) {
  static_assert(sizeof(OctarynCollisionResidencyStats) == 72);
  auto* world = static_cast<octaryn::server::map_world::ServerMapWorld*>(handle);
  if (!world || !stats || byte_size != sizeof(*stats)) return -1;
  *stats = world->tiles ? world->tiles->stats() : OctarynCollisionResidencyStats{2};
  return 0;
}

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
  if (octaryn_server_map_world_collision_ready_state(handle,state,input,delta_seconds)!=0) return 0;
  // Input intent is telemetry, not a simulation gate: idle players still fall.

  auto motion_input = std::bit_cast<octaryn::character_motion::Input>(*input);
  if (result->tick_input == 0u) {
    // Before the first controller command, default input angles are not a look.
    motion_input.camera_pitch = state->pitch;
    motion_input.camera_yaw = state->yaw;
  }
  auto motion_state = std::bit_cast<octaryn::character_motion::State>(*state);
  const auto mesh = world->collision();
  octaryn::character_motion::step_on_mesh(
      motion_input, clamp_delta_seconds(delta_seconds), motion_state, mesh);
  *state = std::bit_cast<OctarynServerPlayerState>(motion_state);

  result->delta_x = state->x - previous_x;
  result->delta_y = state->y - previous_y;
  result->delta_z = state->z - previous_z;
  return 0;
}

} // extern "C"
