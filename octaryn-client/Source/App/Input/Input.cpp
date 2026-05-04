#include "Input.h"

#include "Environment.h"
#include "Log.h"
#include "Window.h"
#include "PlayerControlInput.h"

#include <cinttypes>
#include <cstdio>

namespace octaryn_client_app {

namespace {

constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr float kFlyFastSpeedBlocksPerSecond = 100.0f;
constexpr float kMouseSensitivityDegrees = 0.1f;

bool key_down(const client_key_state &keys, SDL_Scancode scancode) {
  return keys[scancode];
}

void fill_player_control_input(
    player_control_input &control_input,
    const client_input_debug_state &input,
    const fly_player_controller &controller) {
  player_control_input_clear(&control_input);
  control_input.move_right = input.move_x > 0.0f ? 1 : 0;
  control_input.move_left = input.move_x < 0.0f ? 1 : 0;
  control_input.move_up = input.move_y > 0.0f ? 1 : 0;
  control_input.move_down = input.move_y < 0.0f ? 1 : 0;
  control_input.move_forward = input.move_z > 0.0f ? 1 : 0;
  control_input.move_backward = input.move_z < 0.0f ? 1 : 0;
  control_input.sprint = (input.flags & kInputSprintFlag) != 0u ? 1 : 0;
  const float sensitivity = controller.mouse_sensitivity_degrees_per_pixel;
  if (sensitivity > 0.0f) {
    control_input.mouse_yaw_delta = input.look_yaw / sensitivity;
    control_input.mouse_pitch_delta = -input.look_pitch / sensitivity;
  }
}

} // namespace

client_input_debug_state read_client_input(
    SDL_Window *window, const pointer_motion_debug_state &pointer_motion,
    const pointer_click_debug_state &pointer_click,
    const client_key_state &keys) {
  client_input_debug_state input{};
  input.move_x = (key_down(keys, SDL_SCANCODE_D) ? 1.0f : 0.0f) -
                 (key_down(keys, SDL_SCANCODE_A) ? 1.0f : 0.0f);
  input.move_y =
      (key_down(keys, SDL_SCANCODE_SPACE) || key_down(keys, SDL_SCANCODE_E)
           ? 1.0f
           : 0.0f) -
      (key_down(keys, SDL_SCANCODE_Q) || key_down(keys, SDL_SCANCODE_LSHIFT) ||
               key_down(keys, SDL_SCANCODE_RSHIFT)
           ? 1.0f
           : 0.0f);
  input.move_z = (key_down(keys, SDL_SCANCODE_W) ? 1.0f : 0.0f) -
                 (key_down(keys, SDL_SCANCODE_S) ? 1.0f : 0.0f);

  if (key_down(keys, SDL_SCANCODE_SPACE)) {
    input.flags |= kInputJumpFlag;
  }
  if (key_down(keys, SDL_SCANCODE_LCTRL) ||
      key_down(keys, SDL_SCANCODE_RCTRL)) {
    input.flags |= kInputSprintFlag;
    input.speed = kFlyFastSpeedBlocksPerSecond;
  }
  input.flags |= kInputFlyModeFlag;

  if (pointer_click.primary) {
    input.flags |= kInputPrimaryFlag;
  }
  if (pointer_click.secondary) {
    input.flags |= kInputSecondaryFlag;
  }
  input.relative_mouse = SDL_GetWindowRelativeMouseMode(window) ? 1 : 0;
  if (input.relative_mouse != 0) {
    input.look_pitch = -pointer_motion.yrel * kMouseSensitivityDegrees;
    input.look_yaw = pointer_motion.xrel * kMouseSensitivityDegrees;
  }
  input.active =
      input.move_x != 0.0f || input.move_y != 0.0f || input.move_z != 0.0f ||
      input.look_pitch != 0.0f || input.look_yaw != 0.0f ||
      (input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u ||
      input.relative_mouse != 0;
  return input;
}

void apply_input_probe(client_input_debug_state &input, uint64_t frame_index) {
  if (!read_enabled_flag(kInputProbeFlag) || frame_index != 1u) {
    return;
  }

  input.controller = 1u;
  input.move_x = 1.0f;
  input.move_y = 1.0f;
  input.move_z = 1.0f;
  input.look_pitch = -6.0f;
  input.look_yaw = 12.0f;
  input.speed = kFlyFastSpeedBlocksPerSecond;
  input.relative_mouse = 1;
  input.flags |= kInputJumpFlag | kInputSprintFlag | kInputFlyModeFlag |
                 kInputPrimaryFlag | kInputSecondaryFlag;
  input.active = true;
}

octaryn_host_frame_snapshot create_frame(uint64_t frame_index,
                                         double delta_seconds) {
  octaryn_host_frame_snapshot frame{};
  frame.version = 1u;
  frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame.input.version = 1u;
  frame.input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
  frame.timing.version = 1u;
  frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame.timing.frame_index = frame_index;
  frame.timing.delta_seconds = delta_seconds;
  return frame;
}

void apply_input_to_frame(octaryn_host_frame_snapshot &frame,
                          const client_input_debug_state &input,
                          const octaryn_client_camera &camera) {
  frame.input.flags = input.flags;
  frame.input.controller = input.controller;
  frame.input.move_x = input.move_x;
  frame.input.move_y = input.move_y;
  frame.input.move_z = input.move_z;
  frame.input.camera_x = camera.position[0];
  frame.input.camera_y = camera.position[1];
  frame.input.camera_z = camera.position[2];
  frame.input.camera_pitch = camera.pitch_radians;
  frame.input.camera_yaw = camera.yaw_radians;
  frame.input.relative_mouse = input.relative_mouse;
}

void log_client_tick_input_frame(const octaryn_host_frame_snapshot &frame) {
  if (g_log == nullptr) {
    return;
  }

  std::fprintf(g_log,
               "live_client_tick_input frame=%" PRIu64 " dt=%.6f flags=%" PRIu32
               " controller=%" PRIu32 " move=(%.3f,%.3f,%.3f)"
               " camera=(%.3f,%.3f,%.3f,%.6f,%.6f)"
               " relative_mouse=%" PRId32 "\n",
               frame.timing.frame_index, frame.timing.delta_seconds,
               frame.input.flags, frame.input.controller, frame.input.move_x,
               frame.input.move_y, frame.input.move_z, frame.input.camera_x,
               frame.input.camera_y, frame.input.camera_z,
               frame.input.camera_pitch, frame.input.camera_yaw,
               frame.input.relative_mouse);
  std::fflush(g_log);
}

bool update_client_player_controller(
    SDL_Window *window, fly_player_controller &controller,
    const client_input_debug_state &input, double delta_seconds) {
  int render_width = 0;
  int render_height = 0;
  if (!window_output_size(window, &render_width, &render_height)) {
    return false;
  }

  fly_player_controller_resize_viewport(
      &controller, render_width, render_height);
  player_control_input control_input{};
  fill_player_control_input(control_input, input, controller);
  fly_player_controller_update(
      &controller, &control_input, static_cast<float>(delta_seconds));
  return true;
}

double frame_delta_seconds(uint64_t previous_ticks, uint64_t current_ticks) {
  if (previous_ticks == 0u || current_ticks <= previous_ticks) {
    return kDefaultDeltaSeconds;
  }

  return static_cast<double>(current_ticks - previous_ticks) / 1000000000.0;
}

} // namespace octaryn_client_app
