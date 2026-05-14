#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "render/resources/resources.h"

auto main_render_atlas_create_samplers(main_render_resources_t* resources, SDL_GPUDevice* device) -> bool;
auto main_render_atlas_load_animations_internal(main_render_resources_t* resources) -> bool;
void main_render_atlas_update_animations_internal(main_render_resources_t* resources, SDL_GPUDevice* device, Uint64 ticks_ns);
void main_render_atlas_set_window_icon_internal(const main_render_resources_t* resources,
                                                SDL_Window* window,
                                                block_t block);
void main_render_atlas_destroy_internal(main_render_resources_t* resources, SDL_GPUDevice* device);
