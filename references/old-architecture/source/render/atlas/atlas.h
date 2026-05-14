#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "render/resources/resources.h"

bool main_render_atlas_init(main_render_resources_t* resources, SDL_GPUDevice* device);
void main_render_atlas_update_animations(main_render_resources_t* resources, SDL_GPUDevice* device, Uint64 ticks_ns);
void main_render_atlas_set_window_icon(const main_render_resources_t* resources, SDL_Window* window, block_t block);
void main_render_atlas_destroy(main_render_resources_t* resources, SDL_GPUDevice* device);
