#include <SDL3/SDL.h>

#include "world/runtime/internal.h"
#include "world/runtime/private.h"

static block_t set_chunk_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    world_chunk_to_local(chunk, &bx, &by, &bz);
    SDL_LockMutex(chunk->data_mutex);
    block_t old_block = world_chunk_read_local_block(chunk, bx, by, bz);
    world_chunk_write_local_block(chunk, bx, by, bz, block);
    SDL_UnlockMutex(chunk->data_mutex);
    return old_block;
}

static void seed_chunk_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    world_chunk_to_local(chunk, &bx, &by, &bz);
    SDL_LockMutex(chunk->data_mutex);
    world_chunk_write_local_block(chunk, bx, by, bz, block);
    SDL_UnlockMutex(chunk->data_mutex);
}

block_t world_chunk_set_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    return set_chunk_block(chunk, bx, by, bz, block);
}

void world_chunk_seed_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    seed_chunk_block(chunk, bx, by, bz, block);
}
