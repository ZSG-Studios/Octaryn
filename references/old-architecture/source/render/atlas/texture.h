#pragma once

#include <SDL3/SDL.h>

#include "render/resources/resources.h"
#include "render/atlas/config.h"

auto main_render_atlas_mip_levels(void) -> Uint32;
auto main_render_atlas_create_texture(main_render_resources_t* resources, SDL_GPUDevice* device) -> bool;
