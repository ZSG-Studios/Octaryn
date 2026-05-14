#include "world/block/block.h"

int block_get_skylight_opacity(block_t block)
{
    switch (block)
    {
    case BLOCK_EMPTY:
    case BLOCK_BUSH:
    case BLOCK_BLUEBELL:
    case BLOCK_GARDENIA:
    case BLOCK_ROSE:
    case BLOCK_LAVENDER:
    case BLOCK_GLASS:
    case BLOCK_CLOUD:
        return 0;
    case BLOCK_LEAVES:
        return 1;
    case BLOCK_WATER:
    case BLOCK_WATER_1:
    case BLOCK_WATER_2:
    case BLOCK_WATER_3:
    case BLOCK_WATER_4:
    case BLOCK_WATER_5:
    case BLOCK_WATER_6:
    case BLOCK_WATER_7:
    case BLOCK_LAVA:
    case BLOCK_LAVA_1:
    case BLOCK_LAVA_2:
    case BLOCK_LAVA_3:
    case BLOCK_LAVA_4:
    case BLOCK_LAVA_5:
    case BLOCK_LAVA_6:
    case BLOCK_LAVA_7:
        return 2;
    default:
        return block_has_occlusion(block) ? 15 : 0;
    }
}
