#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_profile.h"
#include "render/scene/passes.h"

void app_execute_scene_passes(SDL_GPUCommandBuffer* cbuf,
                              main_render_pass_context_t* render_context,
                              main_frame_profile_sample_t* profile_sample);
