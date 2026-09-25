#include "Controls.h"
#include "LocalSession.h"
#include "FramePacingDisplay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {

void take_jump_input(WorldControls& controls, LocalPlayerInput& input) {
  input.jump_events=controls.jump_events.take();
  input.has_jump_events=controls.jump_events_enabled;
}

void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive) {
  controls.jump_events_enabled=interactive;
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
      controls.jump_events.event(event,false);
      controls.captured = false;
      if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN))SDL_SetWindowRelativeMouseMode(window, false);
    }
    if (!interactive) { controls.jump_events.event(event,false); continue; }
    const auto action = runtime_controls_handle_event(&controls.ui, window, &event);
    if (action & RUNTIME_CONTROLS_FLIGHT_TOGGLED) controls.flying = !controls.flying;
    if (action & RUNTIME_CONTROLS_ZOOM_CYCLED) controls.zoom = (controls.zoom + 1) % 3;
    if (action & RUNTIME_CONTROLS_FULLSCREEN_TOGGLED) {}
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
      if (event.key.key==SDLK_F4 && !(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)) {}
      if (event.key.key==SDLK_F4) controls.third_person=!controls.third_person;
      else if (event.key.key==SDLK_V) controls.shoulder=opposite_shoulder(controls.shoulder);
      else if (event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS)
        ++controls.time_hour_steps;
      else if (event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS)
        --controls.time_hour_steps;
    }
    const bool jump_allowed=!controls.captured && !controls.flying &&
        (SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS);
    controls.jump_events.event(event,jump_allowed);
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
