#pragma once

#include "octaryn_client_runtime_controls.h"

#ifdef __cplusplus
extern "C" {
#endif

int octaryn_client_runtime_settings_load(SDL_Window* window, octaryn_client_runtime_controls* controls);
int octaryn_client_runtime_settings_save(SDL_Window* window, const octaryn_client_runtime_controls* controls);

#ifdef __cplusplus
}
#endif
