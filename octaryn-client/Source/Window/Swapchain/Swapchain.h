#pragma once

#include <SDL3/SDL.h>

#include "FramePacing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct swapchain_state
{
    SDL_GPUPresentMode present_mode;
}
swapchain_state;

void swapchain_state_init(swapchain_state* state);
int swapchain_configure(
    swapchain_state* state,
    SDL_GPUDevice* device,
    SDL_Window* window,
    frame_pacing* frame_pacing);
const char* swapchain_present_mode_name(
    const swapchain_state* state);
const char* swapchain_present_mode_value_name(
    SDL_GPUPresentMode present_mode);

#ifdef __cplusplus
}
#endif
