#pragma once

#include <SDL3/SDL.h>

#include "world/runtime/private.h"

chunk_t* world_create_chunk(SDL_GPUDevice* device);
void world_free_chunk(chunk_t* chunk);
void world_set_chunk_block_function(void* userdata, int bx, int by, int bz, block_t block);
void world_seed_chunk_block_direct_function(void* userdata, int bx, int by, int bz, block_t block);
