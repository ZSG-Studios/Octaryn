#pragma once

#include <SDL3/SDL.h>

SDL_AppResult app_runtime_init(void** appstate, int argc, char** argv);
void app_runtime_quit(void* appstate, SDL_AppResult result);
SDL_AppResult app_runtime_frame(void* appstate);
SDL_AppResult app_runtime_handle_event(void* appstate, SDL_Event* event);
