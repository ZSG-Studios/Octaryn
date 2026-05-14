#include "world/block/block.h"
#include "core/profile.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"
#include "world/runtime/world.h"

bool world_try_get_surface_height(int x, int z, int* surface_y, block_t* surface_block)
{
    OCT_PROFILE_ZONE("world.surface_height_query");
    int position[3] = {x, 0, z};
    int chunk_x = 0;
    int chunk_z = 0;
    chunk_t* chunk = nullptr;
    if (!world_try_get_loaded_chunk_at(position, &chunk_x, &chunk_z, &chunk))
    {
        return false;
    }

    int local_x = x;
    int local_y = 0;
    int local_z = z;
    world_chunk_to_local(chunk, &local_x, &local_y, &local_z);
    SDL_LockMutex(chunk->data_mutex);
    for (int by = CHUNK_HEIGHT - 1; by >= 0; --by)
    {
        const block_t block = world_chunk_read_local_block(chunk, local_x, by, local_z);
        if (!block_is_solid(block) || block == BLOCK_CLOUD)
        {
            continue;
        }
        if (surface_y)
        {
            *surface_y = by;
        }
        if (surface_block)
        {
            *surface_block = block;
        }
        SDL_UnlockMutex(chunk->data_mutex);
        return true;
    }
    SDL_UnlockMutex(chunk->data_mutex);
    return false;
}
