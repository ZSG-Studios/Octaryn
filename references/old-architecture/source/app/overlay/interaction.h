#pragma once

#include <SDL3/SDL.h>

bool app_any_overlay_active(void);
void app_sync_relative_mouse_mode_for_ui(void);
void app_set_lighting_tuning_visible(bool visible);
bool app_handle_global_keydown(SDL_Scancode scancode);
