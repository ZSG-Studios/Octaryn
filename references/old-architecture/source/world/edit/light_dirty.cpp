#include "world/edit/light_dirty.h"

#include "world/runtime/internal.h"

namespace {

void mark_chunk(bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH], int chunk_x, int chunk_z)
{
    if (chunk_x < 0 || chunk_x >= MAX_WORLD_WIDTH || chunk_z < 0 || chunk_z >= MAX_WORLD_WIDTH)
    {
        return;
    }
    dirty_chunks[chunk_x][chunk_z] = true;
}

} // namespace

Uint32 world_edit_get_light_dirty_flags(block_t old_block, block_t new_block)
{
    Uint32 light_flags = 0;
    if (block_get_skylight_opacity(new_block) != block_get_skylight_opacity(old_block))
    {
        light_flags |= WORLD_LIGHT_DIRTY_SKYLIGHT;
    }
    return light_flags;
}

void world_edit_accumulate_skylight_regen_chunks(bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                                                 int chunk_x,
                                                 int chunk_z,
                                                 int local_x,
                                                 int local_z)
{
    mark_chunk(dirty_chunks, chunk_x, chunk_z);

    const bool west = local_x == 0;
    const bool east = local_x == CHUNK_WIDTH - 1;
    const bool north = local_z == 0;
    const bool south = local_z == CHUNK_WIDTH - 1;

    if (west)
    {
        mark_chunk(dirty_chunks, chunk_x - 1, chunk_z);
    }
    if (east)
    {
        mark_chunk(dirty_chunks, chunk_x + 1, chunk_z);
    }
    if (north)
    {
        mark_chunk(dirty_chunks, chunk_x, chunk_z - 1);
    }
    if (south)
    {
        mark_chunk(dirty_chunks, chunk_x, chunk_z + 1);
    }
    if (west && north)
    {
        mark_chunk(dirty_chunks, chunk_x - 1, chunk_z - 1);
    }
    if (west && south)
    {
        mark_chunk(dirty_chunks, chunk_x - 1, chunk_z + 1);
    }
    if (east && north)
    {
        mark_chunk(dirty_chunks, chunk_x + 1, chunk_z - 1);
    }
    if (east && south)
    {
        mark_chunk(dirty_chunks, chunk_x + 1, chunk_z + 1);
    }
}
