#include "EventPump.h"

#include "Log.h"
#include "Window.h"
#include "octaryn_client_function_profile.h"
#include "octaryn_client_runtime_settings.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

namespace octaryn_client_app {
namespace {

using octaryn::client::rendering::block_atlas_scroll_placeable_block;
using octaryn::client::rendering::block_atlas_top_layer_for_block;

constexpr std::array<double, 7> kWorldTimeSpeedMultipliers{
    0.0, 1.0, 60.0, 300.0, 1200.0, 6000.0, 24000.0};

int32_t clamp_int32(int32_t value, int32_t minimum, int32_t maximum) {
  return std::min(std::max(value, minimum), maximum);
}

void apply_menu_settings(
    SDL_Window *window, SDL_GPUDevice *gpu_device,
    octaryn_client_frame_pacing &frame_pacing,
    octaryn_client_swapchain_state &swapchain_state,
    runtime_controls &runtime_controls) {
  const int32_t present_mode_index =
      clamp_int32(runtime_controls.present_mode_index, 0, 2);
  frame_pacing.requested_present_mode =
      present_mode_index == 0
          ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_IMMEDIATE
          : (present_mode_index == 1 ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_MAILBOX
                                     : OCTARYN_CLIENT_PRESENT_MODE_POLICY_VSYNC);
  if (octaryn_client_swapchain_configure(&swapchain_state, gpu_device, window,
                                         &frame_pacing) &&
      g_log != nullptr) {
    std::fprintf(
        g_log,
        "gpu_swapchain_configure=0 source=menu present_mode=%s fps_cap=%d\n",
        octaryn_client_swapchain_present_mode_name(&swapchain_state),
        frame_pacing.fps_cap);
    std::fflush(g_log);
  }
  if (octaryn_client_runtime_settings_save(window, &runtime_controls) == 0) {
    log_line("client_settings_save=failed");
  } else {
    log_line("client_settings_save=0");
  }
}

bool handle_world_time_key(const SDL_Event &event,
                           client_world_time_controls &world_time_controls) {
  if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
    return false;
  }

  const bool speed_up = event.key.scancode == SDL_SCANCODE_EQUALS ||
                        event.key.scancode == SDL_SCANCODE_KP_PLUS;
  const bool slow_down = event.key.scancode == SDL_SCANCODE_MINUS ||
                         event.key.scancode == SDL_SCANCODE_KP_MINUS;
  if (!speed_up && !slow_down) {
    return false;
  }

  const int32_t max_index =
      static_cast<int32_t>(kWorldTimeSpeedMultipliers.size()) - 1;
  world_time_controls.speed_index = clamp_int32(
      world_time_controls.speed_index + (speed_up ? 1 : -1), 0, max_index);
  world_time_controls.speed_multiplier =
      kWorldTimeSpeedMultipliers[static_cast<size_t>(
          world_time_controls.speed_index)];
  world_time_controls.dirty = true;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_world_time_control speed_index=%d "
                 "speed_multiplier=%.3f\n",
                 world_time_controls.speed_index,
                 world_time_controls.speed_multiplier);
    std::fflush(g_log);
  }
  return true;
}

void update_key_state(const SDL_Event &event, client_key_state &keys) {
  if (event.key.scancode >= 0 &&
      event.key.scancode < static_cast<SDL_Scancode>(keys.size())) {
    keys[static_cast<size_t>(event.key.scancode)] =
        event.type == SDL_EVENT_KEY_DOWN;
  }
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_input_event type=%" PRIu32 " scancode=%d "
                 "repeat=%d down=%d\n",
                 static_cast<uint32_t>(event.type),
                 static_cast<int>(event.key.scancode),
                 event.key.repeat ? 1 : 0,
                 event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
    std::fflush(g_log);
  }
}

void update_block_selection(
    const SDL_Event &event, block_selection_state &block_selection,
    const octaryn::client::rendering::BlockAtlas &atlas) {
  const float wheel_y = event.wheel.y;
  const int delta = wheel_y > 0.0f ? 1 : (wheel_y < 0.0f ? -1 : 0);
  if (delta == 0) {
    return;
  }

  block_selection.selected_block = block_atlas_scroll_placeable_block(
      atlas, block_selection.selected_block, delta);
  ++block_selection.change_count;
  if (g_log != nullptr) {
    const int32_t layer =
        block_atlas_top_layer_for_block(atlas, block_selection.selected_block);
    std::fprintf(g_log,
                 "live_selected_block block=%u layer=%d wheel_delta=%d "
                 "changes=%" PRIu64 "\n",
                 static_cast<unsigned>(block_selection.selected_block), layer,
                 delta, block_selection.change_count);
    std::fflush(g_log);
  }
}

void update_pointer_button(const SDL_Event &event, SDL_Window *window,
                           pointer_click_debug_state &pointer_click) {
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
      !SDL_GetWindowRelativeMouseMode(window)) {
    SDL_SetWindowRelativeMouseMode(window, true);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if (event.button.button == SDL_BUTTON_LEFT) {
      pointer_click.primary = true;
    } else if (event.button.button == SDL_BUTTON_RIGHT) {
      pointer_click.secondary = true;
    }
  }
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_pointer_event type=%" PRIu32 " button=%u "
                 "x=%.1f y=%.1f relative=%d\n",
                 static_cast<uint32_t>(event.type),
                 static_cast<unsigned>(event.button.button), event.button.x,
                 event.button.y, SDL_GetWindowRelativeMouseMode(window) ? 1 : 0);
    std::fflush(g_log);
  }
}

void update_pointer_motion(const SDL_Event &event, SDL_Window *window,
                           pointer_motion_debug_state &pointer_motion) {
  if (SDL_GetWindowRelativeMouseMode(window)) {
    pointer_motion.xrel += event.motion.xrel;
    pointer_motion.yrel += event.motion.yrel;
  }
  if (g_log != nullptr &&
      (event.motion.xrel != 0.0f || event.motion.yrel != 0.0f)) {
    std::fprintf(g_log,
                 "live_pointer_motion xrel=%.3f yrel=%.3f relative=%d\n",
                 event.motion.xrel, event.motion.yrel,
                 SDL_GetWindowRelativeMouseMode(window) ? 1 : 0);
    std::fflush(g_log);
  }
}

} // namespace

void poll_events(
    SDL_Window *window, SDL_GPUDevice *gpu_device,
    octaryn_client_frame_pacing &frame_pacing,
    octaryn_client_swapchain_state &swapchain_state,
    runtime_controls &runtime_controls, client_key_state &keys,
    client_world_time_controls &world_time_controls,
    block_selection_state &block_selection,
    const octaryn::client::rendering::BlockAtlas &atlas,
    bool game_modules_disabled, pointer_motion_debug_state &pointer_motion,
    pointer_click_debug_state &pointer_click, bool &running,
    uint64_t frame_index) {
  SDL_Event event{};
  octaryn_client_function_profile_scope profile_scope("event_poll_loop",
                                                      frame_index, "");
  while (SDL_PollEvent(&event)) {
    int event_width = 0;
    int event_height = 0;
    window_output_size(window, &event_width, &event_height);
    if (handle_world_time_key(event, world_time_controls)) {
      continue;
    }

    const uint32_t control_result =
        runtime_controls_handle_event(
            &runtime_controls, window, &event, event_width, event_height);
    if (control_result != 0u && g_log != nullptr) {
      std::fprintf(
          g_log,
          "live_runtime_control_event flags=%" PRIu32
          " debug=%u menu=%u fullscreen=%d render_distance=%d\n",
          control_result,
          static_cast<unsigned>(runtime_controls.debug_overlay_enabled),
          static_cast<unsigned>(runtime_controls.display_menu.active),
          (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0u ? 1 : 0,
          runtime_controls.render_distance);
      std::fflush(g_log);
    }
    if ((control_result & RUNTIME_CONTROLS_QUIT_REQUESTED) !=
        0u) {
      running = false;
    }
    if ((control_result & RUNTIME_CONTROLS_MENU_APPLIED) != 0u) {
      apply_menu_settings(window, gpu_device, frame_pacing, swapchain_state,
                          runtime_controls);
    }
    if ((control_result & RUNTIME_CONTROLS_EVENT_CAPTURED) !=
        0u) {
      continue;
    }

    if (event.type == SDL_EVENT_QUIT ||
        event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "live_window_event type=%" PRIu32 "\n",
                     static_cast<uint32_t>(event.type));
        std::fflush(g_log);
      }
      running = false;
    } else if (event.type == SDL_EVENT_KEY_DOWN ||
               event.type == SDL_EVENT_KEY_UP) {
      update_key_state(event, keys);
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (!game_modules_disabled) {
        update_block_selection(event, block_selection, atlas);
      }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      update_pointer_button(event, window, pointer_click);
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
      update_pointer_motion(event, window, pointer_motion);
    } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
               event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "live_window_size type=%" PRIu32
                            " width=%d height=%d\n",
                     static_cast<uint32_t>(event.type), event.window.data1,
                     event.window.data2);
        std::fflush(g_log);
      }
    }
  }
}

} // namespace octaryn_client_app
