#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"

void world_edit_add_hidden_render_block(const int position[3], int chunk_world_x, int chunk_world_z, int target_mesh_epoch);
void world_edit_remove_hidden_render_block(const int position[3]);

void world_edit_begin_trace(const int position[3],
                            int chunk_world_x,
                            int chunk_world_z,
                            block_t expected_block,
                            Uint32 light_dirty_flags,
                            int target_mesh_epoch,
                            int target_light_epoch);
