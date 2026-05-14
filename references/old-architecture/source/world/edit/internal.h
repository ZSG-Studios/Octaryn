#pragma once

#include "world/runtime/world.h"

typedef struct world_block_edit_apply_result
{
    bool applied;
    bool changed;
}
world_block_edit_apply_result_t;

bool world_try_apply_block_edit(const int position[3], block_t block);
world_block_edit_apply_result_t world_try_apply_block_edit_detailed(const int position[3], block_t block);
