#include "Controls.h"
#include "DebugOverlay.h"
#include "FramePacingDisplay.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::app {

void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive) {
  constexpr float mouse_radians_per_count = 0.1f * SDL_PI_F / 180.0f;
  controls.resized = false;
  controls.display_changed = false;
  controls.time_hour_steps = 0;
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  SDL_Event event;
  const auto window_id = SDL_GetWindowID(window);
  while (SDL_PollEvent(&event)) {
    controls.display_changed |= frame_pacing_display_changed(event, window_id);
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
      controls.running = false;
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      controls.resized = true;
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
      controls.captured = false;
      SDL_SetWindowRelativeMouseMode(window, false);
    }
    if (!interactive) continue;
    const auto action = runtime_controls_handle_event(&controls.ui, window, &event);
    if (action & RUNTIME_CONTROLS_FLIGHT_TOGGLED) controls.flying = !controls.flying;
    if (action & RUNTIME_CONTROLS_ZOOM_CYCLED) controls.zoom = (controls.zoom + 1) % 3;
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
      if (event.key.key==SDLK_F4) controls.third_person=!controls.third_person;
      else if (event.key.key==SDLK_V) controls.shoulder=opposite_shoulder(controls.shoulder);
      else if (event.key.key==SDLK_ESCAPE && controls.captured) {
        controls.captured=false;
        SDL_SetWindowRelativeMouseMode(window,false);
      }
      else if (event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS)
        ++controls.time_hour_steps;
      else if (event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS)
        --controls.time_hour_steps;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !controls.captured &&
        (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
      controls.captured=true;
      SDL_SetWindowRelativeMouseMode(window,true);
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION && controls.captured) {
      controls.yaw = std::remainder(controls.yaw + event.motion.xrel * mouse_radians_per_count, 6.2831853f);
      controls.pitch = std::clamp(controls.pitch - event.motion.yrel * mouse_radians_per_count, -1.55f, 1.55f);
    }
  }
  player_control_input_clear(&controls.movement);
  if (interactive && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    player_control_input_read_sdl_keyboard(&controls.movement, keys, count);
  }
}

}
