#pragma once

#include "RuntimeControls.h"

#ifdef __cplusplus
extern "C" {
#endif

int runtime_settings_load(SDL_Window* window, runtime_controls* controls);
int runtime_settings_save(SDL_Window* window, const runtime_controls* controls);

#ifdef __cplusplus
}
#endif
