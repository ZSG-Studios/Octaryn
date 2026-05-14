#include "Input.h"

#include "Environment.h"
#include "Log.h"
#include "Window.h"
#include "PlayerControlInput.h"

#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace octaryn_client_app {

namespace {

constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr float kFlyFastSpeedBlocksPerSecond = 100.0f;
constexpr float kMovementProbeSpeedBlocksPerSecond = 45.0f;
constexpr float kMouseSensitivityDegrees = 0.1f;
constexpr double kMovementProbeRouteWarmupSeconds = 1.0;
constexpr double kDefaultMovementProbePhaseSeconds = 8.0;
constexpr double kWideMovementProbePhaseSeconds = 8.0;
constexpr const char *kMovementProbeFlag = "OCTARYN_CLIENT_APP_MOVEMENT_PROBE";
constexpr const char *kMovementProbeRouteEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_ROUTE";

uint64_t g_movement_probe_start_ticks;
int64_t g_movement_probe_last_place_slot = -1;
int64_t g_movement_probe_last_break_slot = -1;

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

double movement_probe_elapsed_seconds() {
  if (g_movement_probe_start_ticks == 0u) {
    g_movement_probe_start_ticks = SDL_GetTicksNS();
  }
  return static_cast<double>(SDL_GetTicksNS() - g_movement_probe_start_ticks) *
         1.0e-9;
}

bool consume_movement_probe_pulse(double elapsed_seconds, double start_seconds,
                                  double interval_seconds,
                                  int64_t &last_slot) {
  if (elapsed_seconds < start_seconds || interval_seconds <= 0.0) {
    return false;
  }
  const auto slot = static_cast<int64_t>(
      std::floor((elapsed_seconds - start_seconds) / interval_seconds));
  if (slot == last_slot) {
    return false;
  }
  last_slot = slot;
  return true;
}

void apply_movement_probe_route(client_input_debug_state &input,
                                uint64_t frame_index) {
  (void)frame_index;
  const char *route = std::getenv(kMovementProbeRouteEnv);
  if (route != nullptr && std::strcmp(route, "straight-after-edits") == 0) {
    input.move_x = 0.0f;
    input.move_y = 0.0f;
    input.move_z = -1.0f;
    return;
  }

  const double elapsed_seconds = movement_probe_elapsed_seconds();
  const double phase_seconds =
      route != nullptr && std::strcmp(route, "wide-box") == 0
          ? kWideMovementProbePhaseSeconds
          : kDefaultMovementProbePhaseSeconds;
  const uint64_t phase =
      elapsed_seconds < kMovementProbeRouteWarmupSeconds
          ? 0u
          : (static_cast<uint64_t>(
                 (elapsed_seconds - kMovementProbeRouteWarmupSeconds) /
                 phase_seconds) %
             4u);
  input.move_y = 0.0f;

  if (route != nullptr && std::strcmp(route, "wide-box") == 0) {
    input.move_x = phase == 1u || phase == 2u ? -1.0f : 1.0f;
    input.move_z = phase >= 2u ? -1.0f : 1.0f;
    return;
  }

  if (route != nullptr && std::strcmp(route, "reverse-box") == 0) {
    input.move_x = phase == 1u || phase == 2u ? 1.0f : -1.0f;
    input.move_z = phase >= 2u ? 1.0f : -1.0f;
    return;
  }

  if (route != nullptr && std::strcmp(route, "long-run") == 0) {
    input.move_x = phase == 2u ? -1.0f : 1.0f;
    input.move_z = phase >= 2u ? -0.35f : 0.35f;
    return;
  }

  input.move_x = phase == 1u || phase == 2u ? -1.0f : 1.0f;
  input.move_z = phase >= 2u ? -1.0f : 1.0f;
}

bool is_straight_movement_probe_route() {
  const char *route = std::getenv(kMovementProbeRouteEnv);
  return route != nullptr && std::strcmp(route, "straight-after-edits") == 0;
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
  const bool launch_probe = read_enabled_flag(kInputProbeFlag);
  const bool movement_probe = read_enabled_flag(kMovementProbeFlag);
  if ((!launch_probe || frame_index != 1u) && !movement_probe) {
    return;
  }

  input.controller = 1u;
  const uint64_t phase = movement_probe ? 0u : ((frame_index - 1u) / 90u) % 4u;
  if (movement_probe) {
    apply_movement_probe_route(input, frame_index);
  } else if (phase == 0u) {
    input.move_x = 1.0f;
    input.move_y = -1.0f;
    input.move_z = 1.0f;
  } else if (phase == 1u) {
    input.move_x = 1.0f;
    input.move_y = 0.0f;
    input.move_z = 1.0f;
  } else if (phase == 2u) {
    input.move_x = -1.0f;
    input.move_y = 0.0f;
    input.move_z = 1.0f;
  } else {
    input.move_x = -1.0f;
    input.move_y = 0.0f;
    input.move_z = -1.0f;
  }
  input.look_pitch = frame_index == 1u ? (movement_probe ? -45.0f : -6.0f) : 0.0f;
  input.look_yaw =
      frame_index == 1u && !is_straight_movement_probe_route() ? 12.0f : 0.0f;
  input.speed = movement_probe ? kMovementProbeSpeedBlocksPerSecond
                               : kFlyFastSpeedBlocksPerSecond;
  input.relative_mouse = 1;
  if (movement_probe) {
    input.flags = 0u;
  } else {
    input.flags |= kInputJumpFlag | kInputFlyModeFlag;
  }
  if (!movement_probe || frame_index > 180u) {
    input.flags |= kInputSprintFlag;
  }
  const double movement_elapsed =
      movement_probe ? movement_probe_elapsed_seconds() : 0.0;
  const bool movement_place_frame =
      movement_probe &&
      consume_movement_probe_pulse(movement_elapsed, 2.0, 3.5,
                                   g_movement_probe_last_place_slot);
  const bool movement_break_frame =
      movement_probe &&
      consume_movement_probe_pulse(movement_elapsed, 3.25, 2.5,
                                   g_movement_probe_last_break_slot);
  if (!movement_probe && frame_index == 1u) {
    input.flags |= kInputPrimaryFlag | kInputSecondaryFlag;
  } else if (movement_place_frame) {
    input.flags |= kInputSecondaryFlag;
  } else if (movement_break_frame) {
    input.flags |= kInputPrimaryFlag;
  }
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
                          const camera &camera) {
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
  const float previous_normal_speed = controller.normal_speed_blocks_per_second;
  const float previous_sprint_speed = controller.sprint_speed_blocks_per_second;
  if (input.speed > 0.0f) {
    controller.normal_speed_blocks_per_second = input.speed;
    controller.sprint_speed_blocks_per_second = input.speed;
  }
  fly_player_controller_update(
      &controller, &control_input, static_cast<float>(delta_seconds));
  controller.normal_speed_blocks_per_second = previous_normal_speed;
  controller.sprint_speed_blocks_per_second = previous_sprint_speed;
  return true;
}

double frame_delta_seconds(uint64_t previous_ticks, uint64_t current_ticks) {
  if (previous_ticks == 0u || current_ticks <= previous_ticks) {
    return kDefaultDeltaSeconds;
  }

  return static_cast<double>(current_ticks - previous_ticks) / 1000000000.0;
}

} // namespace octaryn_client_app
