#include "world/runtime/private.h"

#include "core/check.h"
#include "world/runtime/internal.h"

namespace {

chunk_t* g_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH];
int g_sorted_chunks[MAX_WORLD_WIDTH * MAX_WORLD_WIDTH][2];
int g_world_x = 0;
int g_world_z = 0;
bool g_is_moving = false;
int g_active_world_width = 18;

} // namespace

chunk_t* world_get_chunk_internal(int cx, int cz)
{
    if (world_chunk_is_local_index(cx, cz))
    {
        return g_chunks[cx][cz];
    }
    else
    {
        return NULL;
    }
}

chunk_t* world_get_chunk_slot_internal(int cx, int cz)
{
    if (cx >= 0 && cz >= 0 && cx < MAX_WORLD_WIDTH && cz < MAX_WORLD_WIDTH)
    {
        return g_chunks[cx][cz];
    }
    return NULL;
}

void world_set_chunk_internal(int cx, int cz, chunk_t* chunk)
{
    if (cx >= 0 && cz >= 0 && cx < MAX_WORLD_WIDTH && cz < MAX_WORLD_WIDTH)
    {
        if (chunk)
        {
            chunk->slot_id = static_cast<Uint32>(cx * MAX_WORLD_WIDTH + cz);
        }
        g_chunks[cx][cz] = chunk;
    }
}

void world_get_neighborhood_internal(int cx, int cz, chunk_t* neighborhood[3][3])
{
    for (int i = -1; i <= 1; i++)
    for (int j = -1; j <= 1; j++)
    {
        int x = cx + i;
        int z = cz + j;
        neighborhood[i + 1][j + 1] = world_get_chunk_internal(x, z);
        CHECK(neighborhood[i + 1][j + 1]);
    }
}

int world_active_world_width_internal(void)
{
    return g_active_world_width;
}

void world_set_active_world_width_internal(int world_width)
{
    g_active_world_width = world_width;
}

int world_origin_x_internal(void)
{
    return g_world_x;
}

int world_origin_z_internal(void)
{
    return g_world_z;
}

void world_set_origin_internal(int origin_x, int origin_z)
{
    g_world_x = origin_x;
    g_world_z = origin_z;
}

const int (*world_sorted_chunks_internal(void))[2]
{
    return g_sorted_chunks;
}

int (*world_sorted_chunks_mutable_internal(void))[2]
{
    return g_sorted_chunks;
}

bool world_is_moving_internal(void)
{
    return g_is_moving;
}

void world_set_is_moving_internal(bool moving)
{
    g_is_moving = moving;
}
