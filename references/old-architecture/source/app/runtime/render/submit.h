#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_profile.h"

void app_submit_render_frame(SDL_GPUCommandBuffer* cbuf,
                             SDL_GPUTexture* swapchain_texture,
                             main_frame_profile_sample_t* profile_sample);
