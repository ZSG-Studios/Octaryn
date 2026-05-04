#pragma once

#include "AppSettings.h"

#if defined(OCTARYN_CLIENT_DISPLAY_SETTINGS_USE_SDL3)
#include <SDL3/SDL.h>
#else
#include <stdint.h>

typedef uint32_t SDL_DisplayID;
typedef struct SDL_Window SDL_Window;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int display_settings_display_index(SDL_DisplayID display);
void display_settings_capture(
    app_settings* settings,
    SDL_Window* window);
SDL_DisplayID display_settings_resolve_display(
    const app_settings* settings);
int display_settings_restore_window(
    SDL_Window* window,
    const app_settings* settings);

#ifdef __cplusplus
}
#endif
