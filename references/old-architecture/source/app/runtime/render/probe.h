#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/options.h"

bool app_render_probe_capture_requested(SDL_GPUCommandBuffer* cbuf,
                                        int width,
                                        int height,
                                        app_runtime_probe_options_t* options);
