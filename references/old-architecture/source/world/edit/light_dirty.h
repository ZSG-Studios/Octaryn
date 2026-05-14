#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "world/runtime/world.h"

Uint32 world_edit_get_light_dirty_flags(block_t old_block, block_t new_block);
void world_edit_accumulate_skylight_regen_chunks(bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                                                 int chunk_x,
                                                 int chunk_z,
                                                 int local_x,
                                                 int local_z);
