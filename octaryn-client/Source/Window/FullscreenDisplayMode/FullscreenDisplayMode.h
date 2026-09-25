#pragma once

#if defined(OCTARYN_CLIENT_FULLSCREEN_DISPLAY_MODE_USE_SDL3)
#include <SDL3/SDL.h>
#else
#include <stdint.h>

typedef uint32_t SDL_DisplayID;
typedef struct SDL_DisplayMode SDL_DisplayMode;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int fullscreen_display_mode_best(
    SDL_DisplayID display,
    SDL_DisplayMode* mode);

#ifdef __cplusplus
}
#endif
