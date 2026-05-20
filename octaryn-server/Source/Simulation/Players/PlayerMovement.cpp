#include "PlayerMovement.h"

#include "PlayerJoltMovement.h"

#include <cmath>

namespace {

constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t FlyMode = 1u;
constexpr float NormalFlySpeedBlocksPerSecond = 10.0f;
constexpr float SprintFlySpeedBlocksPerSecond = 100.0f;

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 move_camera_relative(float x, float y, float z, float pitch, float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  const float pitch_sine = std::sin(pitch);
  const float pitch_cosine = std::cos(pitch);
  return Vec3{pitch_cosine * (yaw_sine * z) + yaw_cosine * x,
              y + z * pitch_sine,
              -(pitch_cosine * (yaw_cosine * z) - yaw_sine * x)};
}

} // namespace

namespace octaryn::server::simulation::players {

void move_fly(const OctarynServerPlayerInput &input, float dt,
              OctarynServerPlayerState &state, float pitch, float yaw) {
  const float speed = (input.flags & SprintFlag) != 0u
                          ? SprintFlySpeedBlocksPerSecond
                          : NormalFlySpeedBlocksPerSecond;
  const float distance = speed * dt;
  const Vec3 move =
      move_camera_relative(input.move_x * distance, input.move_y * distance,
                           input.move_z * distance, pitch, yaw);
  state.x += move.x;
  state.y = std::fmax(-1000.0f, std::fmin(state.y + move.y, 1000.0f));
  state.z += move.z;
  state.pitch = pitch;
  state.yaw = yaw;
  state.velocity_x = dt > 0.0f ? move.x / dt : 0.0f;
  state.velocity_y = dt > 0.0f ? move.y / dt : 0.0f;
  state.velocity_z = dt > 0.0f ? move.z / dt : 0.0f;
  state.is_on_ground = 0u;
  state.control_mode = FlyMode;
}

void move_walk(const OctarynServerPlayerInput &input, float dt,
               OctarynServerPlayerState &state, float pitch, float yaw,
               octaryn_server_player_block_query_fn block_query,
               void *context) {
  move_walk_with_jolt(input, dt, state, pitch, yaw, block_query, context);
}

} // namespace octaryn::server::simulation::players
