#include "world/jobs/taskflow/backend.h"
#include "world/jobs/taskflow/internal.h"

#include "core/check.h"
#include "world/chunks/state.h"

bool world_jobs_taskflow_try_update_blocks(int x, int z)
{
    CHECK(world_chunk_is_local_index(x, z));
    chunk_t* chunk = world_get_chunk_internal(x, z);
    if (SDL_GetAtomicInt(&chunk->block_state) != JOB_STATE_REQUESTED)
    {
        return false;
    }
    return world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_BLOCKS);
}

bool world_jobs_taskflow_try_update_meshes_or_lights(int x, int z)
{
    CHECK(world_chunk_is_local_index(x, z));
    chunk_t* chunk = world_get_chunk_internal(x, z);
    if (world_chunk_is_border_index(x, z))
    {
        return false;
    }

    const bool do_mesh = SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_REQUESTED;
    const bool do_light = SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
    if (!do_mesh && !do_light)
    {
        return false;
    }

    chunk_t* neighborhood[3][3] = {};
    world_get_neighborhood_internal(x, z, neighborhood);
    if (!world_chunk_neighborhood_blocks_ready(neighborhood))
    {
        return false;
    }

    const bool needs_first_mesh = !world_chunk_render_ready(chunk);
    if (needs_first_mesh)
    {
        return do_mesh && world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_MESHES);
    }

    if (do_light)
    {
        return world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_LIGHTS);
    }
    if (do_mesh)
    {
        return world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_MESHES);
    }
    return false;
}

bool world_jobs_taskflow_try_update_urgent_chunk(int x, int z)
{
    CHECK(world_chunk_is_local_index(x, z));
    chunk_t* chunk = world_get_chunk_internal(x, z);
    if (world_chunk_is_border_index(x, z) || SDL_GetAtomicInt(&chunk->urgent_priority) == 0)
    {
        return false;
    }

    const bool busy = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING ||
        SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING ||
        SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING;
    const bool pending = SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_REQUESTED ||
        SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_REQUESTED ||
        SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
    if (!busy && !pending)
    {
        SDL_SetAtomicInt(&chunk->urgent_priority, 0);
        return false;
    }

    const bool do_mesh = SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_REQUESTED;
    const bool do_light = SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED;
    if (do_mesh || do_light)
    {
        chunk_t* neighborhood[3][3] = {};
        world_get_neighborhood_internal(x, z, neighborhood);
        bool ready = true;
        for (int i = 0; i < 3 && ready; ++i)
        for (int j = 0; j < 3; ++j)
        {
            if (SDL_GetAtomicInt(&neighborhood[i][j]->block_state) != JOB_STATE_COMPLETED)
            {
                ready = false;
                break;
            }
        }
        if (ready)
        {
            if (do_mesh)
            {
                return world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_MESHES);
            }
            if (do_light)
            {
                return world_jobs_taskflow_detail::try_submit_job(x, z, JOB_TYPE_LIGHTS);
            }
        }
    }

    if (world_jobs_taskflow_try_update_meshes_or_lights(x, z))
    {
        return true;
    }
    return world_jobs_taskflow_try_update_blocks(x, z);
}
