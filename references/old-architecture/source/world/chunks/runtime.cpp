#include <SDL3/SDL.h>

#include "world/runtime/internal.h"
#include "world/runtime/private.h"

void world_mark_chunk_urgent(int x, int z)
{
    if (!world_chunk_is_local_index(x, z) || world_chunk_is_border_index(x, z))
    {
        return;
    }

    chunk_t* chunk = world_get_chunk_internal(x, z);
    if (!chunk)
    {
        return;
    }

    SDL_SetAtomicInt(&chunk->urgent_priority, 1);
}
