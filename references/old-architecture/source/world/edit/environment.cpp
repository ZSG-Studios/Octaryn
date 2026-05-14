#include "world/edit/environment.h"

bool world_edit_is_supported_block(block_t block, const int position[3])
{
    if (block == BLOCK_EMPTY)
    {
        return true;
    }
    if (position[1] <= 0)
    {
        return false;
    }
    int below[3] = {position[0], position[1] - 1, position[2]};
    block_t base = world_get_block(below);
    if (block_requires_grass(block))
    {
        return base == BLOCK_GRASS;
    }
    if (block_requires_solid_base(block))
    {
        return block_is_solid(base);
    }
    return true;
}
