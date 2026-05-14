#include "world/chunks/snapshot.h"

#include <SDL3/SDL.h>

#include "core/check.h"
#include "core/profile.h"

namespace {

constexpr int kChunkBlockCount = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH;

auto local_block_index(int x, int y, int z) -> int
{
    return (x * CHUNK_HEIGHT + y) * CHUNK_WIDTH + z;
}

void lock_neighborhood(chunk_t* neighborhood[3][3])
{
    for (int x = 0; x < 3; ++x)
    for (int z = 0; z < 3; ++z)
    {
        SDL_LockMutex(neighborhood[x][z]->data_mutex);
    }
}

void unlock_neighborhood(chunk_t* neighborhood[3][3])
{
    for (int x = 2; x >= 0; --x)
    for (int z = 2; z >= 0; --z)
    {
        SDL_UnlockMutex(neighborhood[x][z]->data_mutex);
    }
}

} // namespace

auto world_snapshot_local_block(const neighborhood_snapshot_t& snapshot,
                                int chunk_x,
                                int chunk_z,
                                int block_x,
                                int block_y,
                                int block_z) -> block_t
{
    const size_t chunk_offset = static_cast<size_t>(chunk_x * 3 + chunk_z) * kChunkBlockCount;
    return snapshot.blocks[chunk_offset + static_cast<size_t>(local_block_index(block_x, block_y, block_z))];
}

auto world_snapshot_neighborhood_block(const neighborhood_snapshot_t& snapshot,
                                       int bx,
                                       int by,
                                       int bz,
                                       int dx,
                                       int dy,
                                       int dz) -> block_t
{
    int x = bx + dx;
    int y = by + dy;
    int z = bz + dz;
    if (y == CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    if (y == -1)
    {
        return BLOCK_GRASS;
    }
    int chunk_x = 1;
    int chunk_z = 1;
    while (x < 0)
    {
        x += CHUNK_WIDTH;
        --chunk_x;
    }
    while (x >= CHUNK_WIDTH)
    {
        x -= CHUNK_WIDTH;
        ++chunk_x;
    }
    while (z < 0)
    {
        z += CHUNK_WIDTH;
        --chunk_z;
    }
    while (z >= CHUNK_WIDTH)
    {
        z -= CHUNK_WIDTH;
        ++chunk_z;
    }
    return world_snapshot_local_block(snapshot, chunk_x, chunk_z, x, y, z);
}

auto world_capture_neighborhood_snapshot(chunk_t* neighborhood[3][3], bool capture_blocks) -> neighborhood_snapshot_t
{
    OCT_PROFILE_ZONE("world_capture_neighborhood_snapshot");
    neighborhood_snapshot_t snapshot{};
    if (capture_blocks)
    {
        snapshot.blocks.resize(static_cast<size_t>(kChunkBlockCount) * 9u, BLOCK_EMPTY);
    }
    lock_neighborhood(neighborhood);
    for (int x = 0; x < 3; ++x)
    for (int z = 0; z < 3; ++z)
    {
        chunk_t* chunk = neighborhood[x][z];
        if (capture_blocks && chunk->blocks)
        {
            const size_t chunk_offset = static_cast<size_t>(x * 3 + z) * kChunkBlockCount;
            SDL_memcpy(snapshot.blocks.data() + chunk_offset, chunk->blocks, sizeof(block_t) * kChunkBlockCount);
        }
    }
    unlock_neighborhood(neighborhood);
    return snapshot;
}
