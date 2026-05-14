#pragma once

#include "world/runtime/world.h"
#include "world/block/block.h"

bool world_edit_is_supported_block(block_t block, const int position[3]);
bool world_edit_should_update_water_region(const int center[3], block_t old_block, block_t new_block);
void world_edit_update_water_region(const int center[3]);
void world_edit_service_water_updates(Uint64 budget_ns);
