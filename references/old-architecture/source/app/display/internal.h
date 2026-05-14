#pragma once

#include "menu.h"

namespace app_display_internal {

auto get_distance_option_index(const main_display_menu_runtime_t* runtime, int chunks) -> int;
auto get_window_display_index(const main_display_menu_t* menu, SDL_Window* window) -> int;
void refresh_display_modes(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);
void refresh_display_menu(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);
void center_window_on_display(SDL_Window* window, SDL_DisplayID display, int width, int height);
void commit_display_menu(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);

} // namespace app_display_internal
