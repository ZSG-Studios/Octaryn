#include "world/chunks/state.h"

#include <SDL3/SDL.h>

#include "world/runtime/private.h"

bool world_chunk_blocks_ready(const chunk_t* chunk)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->block_state)) == JOB_STATE_COMPLETED;
}

bool world_chunk_mesh_requested(const chunk_t* chunk)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->mesh_state)) == JOB_STATE_REQUESTED;
}

bool world_chunk_light_requested(const chunk_t* chunk)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->light_state)) == JOB_STATE_REQUESTED;
}

bool world_chunk_light_job_active(const chunk_t* chunk)
{
    if (!chunk)
    {
        return false;
    }
    const int state = SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->light_state));
    return state == JOB_STATE_RUNNING || state == JOB_STATE_REQUESTED;
}

bool world_chunk_light_ready(const chunk_t* chunk)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->light_state)) == JOB_STATE_COMPLETED &&
           (chunk->gpu_skylight.size > 0 || chunk->pooled_skylight_count > 0);
}

bool world_chunk_has_uploaded_lighting(const chunk_t* chunk)
{
    if (!chunk)
    {
        return false;
    }

    const int last_uploaded_light_epoch =
        SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->last_uploaded_light_epoch));
    return last_uploaded_light_epoch > 0 && (chunk->gpu_skylight.size > 0 || chunk->pooled_skylight_count > 0);
}

bool world_chunk_has_any_gpu_mesh(const chunk_t* chunk)
{
    return chunk && (chunk->pooled_face_counts[MESH_TYPE_OPAQUE] > 0 ||
                     chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT] > 0 ||
                     chunk->pooled_face_counts[MESH_TYPE_WATER] > 0 ||
                     chunk->pooled_face_counts[MESH_TYPE_LAVA] > 0 ||
                     chunk->pooled_face_counts[MESH_TYPE_SPRITE] > 0 ||
                     chunk->gpu_meshes[MESH_TYPE_OPAQUE].size > 0 ||
                     chunk->gpu_meshes[MESH_TYPE_TRANSPARENT].size > 0 ||
                     chunk->gpu_meshes[MESH_TYPE_WATER].size > 0 ||
                     chunk->gpu_meshes[MESH_TYPE_LAVA].size > 0 ||
                     chunk->gpu_meshes[MESH_TYPE_SPRITE].size > 0);
}

bool world_chunk_render_ready(const chunk_t* chunk)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->render_mesh_valid)) != 0 && world_chunk_has_any_gpu_mesh(chunk);
}

bool world_chunk_scene_ready(const chunk_t* chunk)
{
    return world_chunk_render_ready(chunk);
}

bool world_chunk_neighborhood_blocks_ready(chunk_t* neighborhood[3][3])
{
    for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
    {
        if (!world_chunk_blocks_ready(neighborhood[i][j]))
        {
            return false;
        }
    }
    return true;
}

bool world_chunk_mesh_uploaded_at_least(const chunk_t* chunk, int epoch)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->last_uploaded_mesh_epoch)) >= epoch;
}

bool world_chunk_light_uploaded_at_least(const chunk_t* chunk, int epoch)
{
    return chunk && SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->last_uploaded_light_epoch)) >= epoch;
}
