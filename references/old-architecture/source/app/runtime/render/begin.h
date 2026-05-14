#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_profile.h"

struct app_render_begin_t {
    SDL_GPUCommandBuffer* cbuf = nullptr;
    SDL_GPUTexture* swapchain_texture = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    bool swapchain_unavailable = false;
    bool resize_allowed = true;
};

bool app_begin_render_commands(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample);
bool app_acquire_render_swapchain(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample);
bool app_begin_render_frame(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample);
