#pragma once

#include "octaryn_client_runtime_controls.h"

#if defined(OCTARYN_CLIENT_RUNTIME_CONTROLS_USE_SDL3)

void octaryn_client_runtime_controls_copy_to_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window);
uint32_t octaryn_client_runtime_controls_request_apply(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window);
uint32_t octaryn_client_runtime_controls_close_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window);
uint32_t octaryn_client_runtime_controls_activate_menu_row(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    int32_t row,
    int32_t delta);
int32_t octaryn_client_runtime_controls_hit_menu_row(
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y);

#endif
