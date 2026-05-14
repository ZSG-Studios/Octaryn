#include "world/edit/schedule.h"

#include "world/edit/light_dirty.h"
#include "world/runtime/internal.h"

namespace {

void request_urgent_mesh_regen_if_loaded(int chunk_x, int chunk_z)
{
    if (!world_chunk_is_local_index(chunk_x, chunk_z) || world_chunk_is_border_index(chunk_x, chunk_z))
    {
        return;
    }

    world_regen_chunk_meshes(chunk_x, chunk_z);
    world_mark_chunk_urgent(chunk_x, chunk_z);
}

} // namespace

void world_edit_request_urgent_mesh_regen_for_chunk(int chunk_x, int chunk_z)
{
    request_urgent_mesh_regen_if_loaded(chunk_x, chunk_z);
}

void world_edit_request_urgent_mesh_regen_for_edit(int chunk_x, int chunk_z, int local_x, int local_z)
{
    request_urgent_mesh_regen_if_loaded(chunk_x, chunk_z);
    if (local_x == 0)
    {
        request_urgent_mesh_regen_if_loaded(chunk_x - 1, chunk_z);
    }
    else if (local_x == CHUNK_WIDTH - 1)
    {
        request_urgent_mesh_regen_if_loaded(chunk_x + 1, chunk_z);
    }

    if (local_z == 0)
    {
        request_urgent_mesh_regen_if_loaded(chunk_x, chunk_z - 1);
    }
    else if (local_z == CHUNK_WIDTH - 1)
    {
        request_urgent_mesh_regen_if_loaded(chunk_x, chunk_z + 1);
    }
}

void world_edit_request_skylight_regen_for_chunk(int chunk_x, int chunk_z)
{
    if (!world_chunk_is_local_index(chunk_x, chunk_z) || world_chunk_is_border_index(chunk_x, chunk_z))
    {
        return;
    }

    world_request_chunk_light_regen_flags(chunk_x, chunk_z, WORLD_LIGHT_DIRTY_SKYLIGHT);
}

void world_edit_request_skylight_regen_for_chunks(const bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH])
{
    for (int x = 0; x < MAX_WORLD_WIDTH; ++x)
    for (int z = 0; z < MAX_WORLD_WIDTH; ++z)
    {
        if (dirty_chunks[x][z])
        {
            world_edit_request_skylight_regen_for_chunk(x, z);
        }
    }
}

void world_edit_request_skylight_regen_for_edit(int chunk_x, int chunk_z, int local_x, int local_z)
{
    bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH] = {0};
    world_edit_accumulate_skylight_regen_chunks(dirty_chunks, chunk_x, chunk_z, local_x, local_z);
    world_edit_request_skylight_regen_for_chunks(dirty_chunks);
}
