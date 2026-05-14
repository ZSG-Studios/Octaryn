#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_profile.h"

void app_render_scene(SDL_GPUCommandBuffer* cbuf, main_frame_profile_sample_t* profile_sample);
