#include "MapPlayer.h"
#include "Camera.h"

#include <cmath>

namespace octaryn::client::app {
namespace {

constexpr float FlySpeedBlocksPerSecond = 12.0f;
constexpr float SprintSpeedBlocksPerSecond = 36.0f;
// One in-engine day per 30 real minutes, matching the previous world clock.
constexpr double RealSecondsPerDay = 1800.0;

float normalized_angle(float radians) {
  constexpr float TwoPi = 6.28318530718f;
  float value = std::fmod(radians, TwoPi);
  if (value < 0.0f) value += TwoPi;
  return value;
}

} // namespace

void map_player_spawn(MapPlayer& player, float x, float y, float z, float yaw, float pitch) {
  player.x = x;
  player.y = y;
  player.z = z;
  player.yaw = yaw;
  player.pitch = pitch;
  player.velocity_x = player.velocity_y = player.velocity_z = 0.0f;
  player.day_origin_fraction = player.world_day_fraction;
}

void map_player_update(MapPlayer& player, const player_control_input& input,
                       bool flying, float yaw, float pitch, double elapsed_seconds) {
  player.yaw = yaw;
  player.pitch = pitch;
  player.flying = flying;

  ::camera view{};
  camera_init(&view, CAMERA_PROJECTION_PERSPECTIVE);
  view.pitch_radians = pitch;
  view.yaw_radians = yaw;
  view.position[0] = player.x;
  view.position[1] = player.y;
  view.position[2] = player.z;
  camera_update(&view);
  float forward_x{}, forward_y{}, forward_z{};
  camera_forward_vector(&view, &forward_x, &forward_y, &forward_z);
  // Right = forward x world up (+Y), normalized.
  float right_x = -forward_z;
  float right_z = forward_x;
  const float right_length = std::hypot(right_x, right_z);
  if (right_length > 1e-6f) {
    right_x /= right_length;
    right_z /= right_length;
  }

  float move_x = 0.0f, move_y = 0.0f, move_z = 0.0f;
  if (input.move_forward) { move_x += forward_x; move_y += forward_y; move_z += forward_z; }
  if (input.move_backward) { move_x -= forward_x; move_y -= forward_y; move_z -= forward_z; }
  if (input.move_left) { move_x -= right_x; move_z -= right_z; }
  if (input.move_right) { move_x += right_x; move_z += right_z; }
  if (input.move_up) move_y += 1.0f;
  if (input.move_down) move_y -= 1.0f;

  const float move_length = std::hypot(move_x, move_y, move_z);
  if (move_length > 1e-6f) {
    const float speed = input.sprint ? SprintSpeedBlocksPerSecond : FlySpeedBlocksPerSecond;
    const float scale = speed / move_length * static_cast<float>(elapsed_seconds);
    player.velocity_x = move_x / move_length * speed;
    player.velocity_y = move_y / move_length * speed;
    player.velocity_z = move_z / move_length * speed;
    player.x += move_x * scale;
    player.y += move_y * scale;
    player.z += move_z * scale;
  } else {
    player.velocity_x = player.velocity_y = player.velocity_z = 0.0f;
  }

  player.source_seconds += elapsed_seconds;
  const double day_fraction = std::fmod(player.day_origin_fraction + player.source_seconds / RealSecondsPerDay, 1.0);
  player.world_day_fraction = static_cast<float>(day_fraction < 0.0 ? day_fraction + 1.0 : day_fraction);
  player.yaw = normalized_angle(player.yaw);
}

void map_player_step_hours(MapPlayer& player, int hours) {
  const double stepped = player.day_origin_fraction + static_cast<double>(hours) / 24.0;
  const double wrapped = std::fmod(stepped, 1.0);
  player.day_origin_fraction = static_cast<float>(wrapped < 0.0 ? wrapped + 1.0 : wrapped);
  player.world_day_fraction = player.day_origin_fraction;
}

}
