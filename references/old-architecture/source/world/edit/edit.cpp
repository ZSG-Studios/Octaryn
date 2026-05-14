#include "world/edit/internal.h"

bool world_set_block(const int position[3], block_t block)
{
    return world_try_apply_block_edit(position, block);
}
