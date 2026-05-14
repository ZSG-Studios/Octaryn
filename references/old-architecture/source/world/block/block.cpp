#include "world/block/block.h"

bool block_is_placeable(block_t block)
{
    return block > BLOCK_EMPTY &&
        block != BLOCK_CLOUD &&
        (block < BLOCK_WATER_1 || block > BLOCK_WATER_7) &&
        (block < BLOCK_LAVA_1 || block > BLOCK_LAVA_7);
}

bool block_is_targetable(block_t block)
{
    return block != BLOCK_EMPTY &&
        (block_is_solid(block) ||
         block_is_sprite(block) ||
         block_requires_grass(block) ||
         block_requires_solid_base(block));
}
