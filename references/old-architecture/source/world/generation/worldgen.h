#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"

typedef void (*worldgen_set_block_t)(void* userdata, int bx, int by, int bz, block_t block);

void worldgen_get_blocks(void* userdata, int cx, int cz, worldgen_set_block_t function);
