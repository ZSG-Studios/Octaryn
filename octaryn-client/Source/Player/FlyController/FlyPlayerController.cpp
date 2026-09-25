#include "FlyPlayerController.h"

#include <cmath>

namespace {

constexpr float DefaultNormalSpeedBlocksPerSecond = 10.0f;
constexpr float DefaultSprintSpeedBlocksPerSecond = 100.0f;
constexpr float DefaultMouseSensitivityDegreesPerPixel = 0.1f;
constexpr float DefaultSpawnY = 80.0f;
constexpr float DefaultSpawnPitchRadians = -0.35f;

float input_axis(int positive, int negative) {
  return (positive != 0 ? 1.0f : 0.0f) - (negative != 0 ? 1.0f : 0.0f);
}

} // namespace

void fly_player_controller_init(
    fly_player_controller *controller) {
  if (controller == nullptr) {
    return;
  }

  camera_init(&controller->camera,
                             CAMERA_PROJECTION_PERSPECTIVE);
  controller->normal_speed_blocks_per_second =
      DefaultNormalSpeedBlocksPerSecond;
  controller->sprint_speed_blocks_per_second =
      DefaultSprintSpeedBlocksPerSecond;
  controller->mouse_sensitivity_degrees_per_pixel =
      DefaultMouseSensitivityDegreesPerPixel;
  fly_player_controller_reset_spawn(controller);
}

void fly_player_controller_reset_spawn(
    fly_player_controller *controller) {
  if (controller == nullptr) {
    return;
  }

  controller->camera.position[0] = 0.0f;
  controller->camera.position[1] = DefaultSpawnY;
  controller->camera.position[2] = 0.0f;
  controller->camera.pitch_radians = DefaultSpawnPitchRadians;
  controller->camera.yaw_radians = 0.0f;
  camera_update(&controller->camera);
}

void fly_player_controller_resize_viewport(
    fly_player_controller *controller, int width, int height) {
  if (controller == nullptr) {
    return;
  }

  camera_resize(&controller->camera, width, height);
  camera_update(&controller->camera);
}

void fly_player_controller_set_position(
    fly_player_controller *controller, float x, float y,
    float z) {
  if (controller == nullptr) {
    return;
  }

  controller->camera.position[0] = x;
  controller->camera.position[1] = y;
  controller->camera.position[2] = z;
  camera_update(&controller->camera);
}

void fly_player_controller_update(
    fly_player_controller *controller,
    const player_control_input *input, float delta_seconds) {
  if (controller == nullptr) {
    return;
  }

  if (input != nullptr) {
    camera_rotate_degrees(
        &controller->camera,
        input->mouse_pitch_delta *
            -controller->mouse_sensitivity_degrees_per_pixel,
        input->mouse_yaw_delta *
            controller->mouse_sensitivity_degrees_per_pixel);
  }

  if (input != nullptr && std::isfinite(delta_seconds) &&
      delta_seconds > 0.0f) {
    const float speed = input->sprint != 0
                            ? controller->sprint_speed_blocks_per_second
                            : controller->normal_speed_blocks_per_second;
    const float distance = speed * delta_seconds;
    const float move_x = input_axis(input->move_right, input->move_left);
    const float move_y = input_axis(input->move_up, input->move_down);
    const float move_z = input_axis(input->move_forward, input->move_backward);
    camera_move(&controller->camera, move_x * distance,
                               move_y * distance, move_z * distance);
  }

  camera_update(&controller->camera);
}
