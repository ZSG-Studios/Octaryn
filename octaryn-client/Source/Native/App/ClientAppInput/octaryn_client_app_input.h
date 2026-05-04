#pragma once

#include "octaryn_client_fly_player_controller.h"
#include "octaryn_client_host_exports.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

namespace octaryn_client_app {

constexpr uint32_t kInputJumpFlag = 1u << 0u;
constexpr uint32_t kInputSprintFlag = 1u << 1u;
constexpr uint32_t kInputFlyModeFlag = 1u << 2u;
constexpr uint32_t kInputPrimaryFlag = 1u << 3u;
constexpr uint32_t kInputSecondaryFlag = 1u << 4u;

struct client_input_debug_state {
  uint32_t flags = 0u;
  uint32_t controller = 0u;
  float move_x = 0.0f;
  float move_y = 0.0f;
  float move_z = 0.0f;
  float look_pitch = 0.0f;
  float look_yaw = 0.0f;
  float speed = 10.0f;
  int relative_mouse = 0;
  bool active = false;
};

struct pointer_motion_debug_state {
  float xrel = 0.0f;
  float yrel = 0.0f;
};

struct pointer_click_debug_state {
  bool primary = false;
  bool secondary = false;
};

using client_key_state = std::array<bool, SDL_SCANCODE_COUNT>;

client_input_debug_state read_client_input(
    SDL_Window *window, const pointer_motion_debug_state &pointer_motion,
    const pointer_click_debug_state &pointer_click,
    const client_key_state &keys);
void apply_input_probe(client_input_debug_state &input, uint64_t frame_index);
octaryn_host_frame_snapshot create_frame(uint64_t frame_index,
                                         double delta_seconds);
void apply_input_to_frame(octaryn_host_frame_snapshot &frame,
                          const client_input_debug_state &input,
                          const octaryn_client_camera &camera);
void log_client_tick_input_frame(const octaryn_host_frame_snapshot &frame);
bool update_client_player_controller(
    SDL_Window *window, octaryn_client_fly_player_controller &controller,
    const client_input_debug_state &input, double delta_seconds);
double frame_delta_seconds(uint64_t previous_ticks, uint64_t current_ticks);

} // namespace octaryn_client_app
