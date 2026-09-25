#pragma once

#include <SDL3/SDL.h>

namespace octaryn::client::app {
inline bool frame_pacing_display_changed(const SDL_Event& event, SDL_WindowID window) {
  switch (event.type) {
    case SDL_EVENT_DISPLAY_ADDED:
    case SDL_EVENT_DISPLAY_REMOVED:
    case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
      return true;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
      return event.window.windowID == window;
    default:
      return false;
  }
}

inline double frame_pacing_refresh_rate(SDL_Window* window) {
  const auto display = SDL_GetDisplayForWindow(window);
  const auto* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
  return mode ? double(mode->refresh_rate) : 0.0;
}
}
