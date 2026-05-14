#pragma once

#include "window.h"

namespace app_window_internal {

bool get_fullscreen_mode(SDL_DisplayID display, SDL_DisplayMode* mode);
auto get_present_mode_name(SDL_GPUPresentMode present_mode) -> const char*;

} // namespace app_window_internal
