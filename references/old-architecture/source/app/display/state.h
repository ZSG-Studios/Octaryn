#pragma once

#include "menu.h"

int main_display_menu_mode_pixel_width(const SDL_DisplayMode* mode);
int main_display_menu_mode_pixel_height(const SDL_DisplayMode* mode);
void main_display_menu_open(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);
void main_display_menu_close(main_display_menu_t* menu);
void main_display_menu_adjust(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime, int delta);
