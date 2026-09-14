#include "Controls.h"
#include "RuntimeSettings.h"
#include "LightingPanel.h"
#include "GameUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive) {
  constexpr float mouse_radians_per_count = 0.1f * SDL_PI_F / 180.0f;
  controls.resized = false;
  controls.time_hour_steps = 0;
  controls.actions.clear();
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
      controls.running = false;
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      controls.resized = true;
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
      controls.captured = false;
      SDL_SetWindowRelativeMouseMode(window, false);
    }
    if (!interactive) continue;
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat &&
        (event.key.key==SDLK_F4 || event.key.key==SDLK_V) &&
        !(controls.game_ui && controls.game_ui->modal_open()) &&
        !runtime_controls_ui_active(&controls.ui) && !(controls.lighting && controls.lighting->visible)) {
      if(event.key.key==SDLK_F4) controls.third_person=!controls.third_person;
      else controls.shoulder=opposite_shoulder(controls.shoulder);
      continue;
    }
    const auto action = controls.game_ui ? controls.game_ui->event(event,width,height) : 0u;
    controls.captured = SDL_GetWindowRelativeMouseMode(window);
    if (action & RUNTIME_CONTROLS_QUIT_REQUESTED) controls.running = false;
    if (action & RUNTIME_CONTROLS_FLIGHT_TOGGLED) controls.flying = !controls.flying;
    if (action & RUNTIME_CONTROLS_ZOOM_CYCLED) controls.zoom = (controls.zoom + 1) % 3;
    if (action & (RUNTIME_CONTROLS_MENU_APPLIED | RUNTIME_CONTROLS_FULLSCREEN_TOGGLED))
      runtime_settings_save(window, &controls.ui);
    if (action & RUNTIME_CONTROLS_EVENT_CAPTURED) {
      if(controls.game_ui && controls.game_ui->modal_open()) controls.actions.clear();
      continue;
    }
    if (runtime_controls_ui_active(&controls.ui) || (controls.game_ui && controls.game_ui->modal_open())) {
      controls.actions.clear();continue;
    }
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
      if (!(controls.lighting && controls.lighting->visible)) {
        if (event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS)
          ++controls.time_hour_steps;
        else if (event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS)
          --controls.time_hour_steps;
      }
      if(event.key.key>=SDLK_1 && event.key.key<=SDLK_9)
        controls.actions.push({BlockActionKind::Select,static_cast<int>(event.key.key-SDLK_1)});
      else if(event.key.key==SDLK_0) controls.actions.push({BlockActionKind::Select,9});
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && controls.captured) {
      if(event.button.button==SDL_BUTTON_LEFT) controls.actions.push({BlockActionKind::Break});
      if(event.button.button==SDL_BUTTON_RIGHT) controls.actions.push({BlockActionKind::Place});
      if(event.button.button==SDL_BUTTON_MIDDLE) controls.actions.push({BlockActionKind::Pick});
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL)
      controls.actions.push({BlockActionKind::Cycle,event.wheel.y > 0 ? 1 : event.wheel.y < 0 ? -1 : 0});
    if (event.type == SDL_EVENT_MOUSE_MOTION && controls.captured) {
      controls.yaw = std::remainder(controls.yaw + event.motion.xrel * mouse_radians_per_count, 6.2831853f);
      controls.pitch = std::clamp(controls.pitch - event.motion.yrel * mouse_radians_per_count, -1.55f, 1.55f);
    }
  }
  if(controls.actions.overflowed() && !controls.action_overflow_reported) {
    std::fprintf(stderr,"Block action queue full: preserving first 64 actions; later actions in this frame were dropped.\n");
    controls.action_overflow_reported=true;
  }
  player_control_input_clear(&controls.movement);
  if (controls.game_ui && controls.game_ui->modal_open()) controls.actions.clear();
  if (interactive && !(controls.game_ui && controls.game_ui->modal_open()) && !(controls.lighting && controls.lighting->visible) && !runtime_controls_ui_active(&controls.ui) && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    player_control_input_read_sdl_keyboard(&controls.movement, keys, count);
  }
}
}
