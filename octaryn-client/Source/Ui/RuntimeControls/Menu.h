#pragma once

#include "RuntimeControls.h"

#if defined(RUNTIME_CONTROLS_USE_SDL3)

void runtime_controls_copy_to_menu(
    runtime_controls* controls,
    SDL_Window* window);
uint32_t runtime_controls_request_apply(
    runtime_controls* controls,
    SDL_Window* window);
uint32_t runtime_controls_close_menu(
    runtime_controls* controls,
    SDL_Window* window);
uint32_t runtime_controls_activate_menu_row(
    runtime_controls* controls,
    SDL_Window* window,
    int32_t row,
    int32_t delta);
int32_t runtime_controls_hit_menu_row(
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y);

#endif
