#pragma once

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

int window_lifecycle_toggle_fullscreen(SDL_Window* window);
int window_lifecycle_apply_best_fullscreen(SDL_Window* window);
int window_lifecycle_show(SDL_Window* window);
void window_lifecycle_finish_show(SDL_Window* window);

#ifdef __cplusplus
}
#endif
