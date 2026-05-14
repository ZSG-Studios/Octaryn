#include "world/block/internal.h"

#include "core/check.h"

bool block_is_opaque(block_t block)
{
    return block_definition(block).is_opaque;
}

bool block_is_sprite(block_t block)
{
    return block_definition(block).is_sprite;
}

bool block_is_solid(block_t block)
{
    return block_definition(block).is_solid;
}

bool block_has_occlusion(block_t block)
{
    return block_definition(block).has_occlusion;
}

bool block_requires_grass(block_t block)
{
    switch (block)
    {
    case BLOCK_BUSH:
    case BLOCK_BLUEBELL:
    case BLOCK_GARDENIA:
    case BLOCK_ROSE:
    case BLOCK_LAVENDER:
        return true;
    default:
        return false;
    }
}

bool block_requires_solid_base(block_t block)
{
    switch (block)
    {
    case BLOCK_RED_TORCH:
    case BLOCK_GREEN_TORCH:
    case BLOCK_BLUE_TORCH:
    case BLOCK_YELLOW_TORCH:
    case BLOCK_CYAN_TORCH:
    case BLOCK_MAGENTA_TORCH:
    case BLOCK_WHITE_TORCH:
        return true;
    default:
        return false;
    }
}

int block_get_index(block_t block, direction_t direction)
{
    CHECK(direction >= DIRECTION_NORTH && direction < DIRECTION_COUNT);
    return block_definition(block).indices[direction];
}
