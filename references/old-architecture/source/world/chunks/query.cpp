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

static void chunk_to_world(const chunk_t* chunk, int* bx, int* by, int* bz)
{
    CHECK(*by >= 0 && *by < CHUNK_HEIGHT);
    *bx += chunk->position[0];
    *bz += chunk->position[1];
}

static int floor_chunk_index(float index)
{
    return static_cast<int>(SDL_floorf(index / static_cast<float>(CHUNK_WIDTH)));
}

static block_t get_chunk_block(chunk_t* chunk, int bx, int by, int bz)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    world_chunk_to_local(chunk, &bx, &by, &bz);
    SDL_LockMutex(chunk->data_mutex);
    const block_t block = world_chunk_read_local_block(chunk, bx, by, bz);
    SDL_UnlockMutex(chunk->data_mutex);
    return block;
}

block_t world_get_neighborhood_block_internal(chunk_t* neighborhood[3][3], int bx, int by, int bz, int dx, int dy, int dz)
{
    CHECK(dx >= -1 && dx <= 1);
    CHECK(dy >= -1 && dy <= 1);
    CHECK(dz >= -1 && dz <= 1);
    bx += dx;
    by += dy;
    bz += dz;
    const chunk_t* chunk = neighborhood[1][1];
    if (by == CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    if (by == -1)
    {
        return BLOCK_GRASS;
    }
    if (is_block_local(bx, by, bz))
    {
        return world_chunk_read_local_block(chunk, bx, by, bz);
    }
    chunk_to_world(chunk, &bx, &by, &bz);
    chunk_t* neighbor = neighborhood[dx + 1][dz + 1];
    CHECK(neighbor);
    return get_chunk_block(neighbor, bx, by, bz);
}

block_t world_get_block(const int position[3])
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return BLOCK_EMPTY;
    }
    const int chunk_x = floor_chunk_index(static_cast<float>(position[0]) -
                                          static_cast<float>(world_origin_x_internal()) * static_cast<float>(CHUNK_WIDTH));
    const int chunk_z = floor_chunk_index(static_cast<float>(position[2]) -
                                          static_cast<float>(world_origin_z_internal()) * static_cast<float>(CHUNK_WIDTH));
    chunk_t* chunk = world_get_chunk_internal(chunk_x, chunk_z);
    if (!chunk)
    {
        SDL_Log("Bad chunk position: %d, %d", chunk_x, chunk_z);
        return BLOCK_EMPTY;
    }
    CHECK(chunk->position[0] == (world_origin_x_internal() + chunk_x) * CHUNK_WIDTH);
    CHECK(chunk->position[1] == (world_origin_z_internal() + chunk_z) * CHUNK_WIDTH);
    if (SDL_GetAtomicInt(&chunk->block_state) != JOB_STATE_COMPLETED)
    {
        return BLOCK_EMPTY;
    }
    return get_chunk_block(chunk, position[0], position[1], position[2]);
}

bool world_try_get_loaded_chunk_at(const int position[3], int* chunk_x, int* chunk_z, chunk_t** chunk)
{
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return false;
    }
    *chunk_x = floor_chunk_index(static_cast<float>(position[0]) -
                                 static_cast<float>(world_origin_x_internal()) * static_cast<float>(CHUNK_WIDTH));
    *chunk_z = floor_chunk_index(static_cast<float>(position[2]) -
                                 static_cast<float>(world_origin_z_internal()) * static_cast<float>(CHUNK_WIDTH));
    *chunk = world_get_chunk_internal(*chunk_x, *chunk_z);
    return *chunk && SDL_GetAtomicInt(&(*chunk)->block_state) == JOB_STATE_COMPLETED;
}

bool world_get_block_render_debug(const int position[3], world_block_render_debug_t* out_debug)
{
    if (!out_debug)
    {
        return false;
    }

    *out_debug = {};
    int chunk_x = 0;
    int chunk_z = 0;
    chunk_t* chunk = nullptr;
    if (!world_try_get_loaded_chunk_at(position, &chunk_x, &chunk_z, &chunk))
    {
        out_debug->chunk_x = chunk_x;
        out_debug->chunk_z = chunk_z;
        return false;
    }

    out_debug->loaded = true;
    out_debug->chunk_x = chunk_x;
    out_debug->chunk_z = chunk_z;
    out_debug->slot_id = chunk->slot_id;
    out_debug->opaque_faces = chunk->pooled_face_counts[MESH_TYPE_OPAQUE];
    out_debug->transparent_faces = chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT];
    out_debug->sprite_faces = chunk->pooled_face_counts[MESH_TYPE_SPRITE];
    out_debug->skylight_count = chunk->pooled_skylight_count;
    return true;
}

block_t world_chunk_get_block(chunk_t* chunk, int bx, int by, int bz)
{
    return get_chunk_block(chunk, bx, by, bz);
}
