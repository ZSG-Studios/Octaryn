#include <SDL3/SDL.h>

#include "core/check.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

static bool is_block_local(int bx, int by, int bz)
{
    CHECK(by >= 0 && by < CHUNK_HEIGHT);
    CHECK(bx >= -1 && bz >= -1 && bx <= CHUNK_WIDTH && bz <= CHUNK_WIDTH);
    return bx >= 0 && bz >= 0 && bx < CHUNK_WIDTH && bz < CHUNK_WIDTH;
}

static int block_index(int bx, int by, int bz)
{
    CHECK(bx >= 0 && bx < CHUNK_WIDTH);
    CHECK(by >= 0 && by < CHUNK_HEIGHT);
    CHECK(bz >= 0 && bz < CHUNK_WIDTH);
    return (bx * CHUNK_HEIGHT + by) * CHUNK_WIDTH + bz;
}

bool world_chunk_ensure_blocks_allocated(chunk_t* chunk)
{
    if (chunk->blocks)
    {
        return true;
    }
    chunk->blocks = static_cast<block_t*>(SDL_malloc(sizeof(block_t) * CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH));
    if (!chunk->blocks)
    {
        SDL_Log("Failed to allocate chunk blocks");
        return false;
    }
    return true;
}

void world_chunk_clear_blocks(chunk_t* chunk)
{
    CHECK(chunk->blocks);
    SDL_memset(chunk->blocks, 0, sizeof(block_t) * CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH);
}

block_t world_chunk_read_local_block(const chunk_t* chunk, int bx, int by, int bz)
{
    if (!chunk->blocks)
    {
        return BLOCK_EMPTY;
    }
    return chunk->blocks[block_index(bx, by, bz)];
}

void world_chunk_write_local_block(chunk_t* chunk, int bx, int by, int bz, block_t block)
{
    CHECK(chunk->blocks);
    chunk->blocks[block_index(bx, by, bz)] = block;
}

static bool is_chunk_local(int cx, int cz)
{
    const int active_world_width = world_active_world_width_internal();
    return cx >= 0 && cz >= 0 && cx < active_world_width && cz < active_world_width;
}

static bool is_chunk_on_border(int cx, int cz)
{
    const int active_world_width = world_active_world_width_internal();
    return cx == 0 || cz == 0 || cx == active_world_width - 1 || cz == active_world_width - 1;
}

bool world_chunk_is_local_index(int cx, int cz)
{
    return is_chunk_local(cx, cz);
}

bool world_chunk_is_border_index(int cx, int cz)
{
    return is_chunk_on_border(cx, cz);
}

int world_chunk_world_x(const chunk_t* chunk)
{
    return chunk->position[0];
}

int world_chunk_world_z(const chunk_t* chunk)
{
    return chunk->position[1];
}

void world_chunk_to_local(const chunk_t* chunk, int* bx, int* by, int* bz)
{
    *bx -= chunk->position[0];
    *bz -= chunk->position[1];
    CHECK(is_block_local(*bx, *by, *bz));
}
