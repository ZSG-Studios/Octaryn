#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "core/check.h"
#include "core/log.h"
#include "core/profile.h"
#include "world/chunks/lifecycle.h"
#include "world/chunks/state.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"
#include "world/window/window.h"

namespace {

void request_chunk_mesh_regen(chunk_t* chunk)
{
    const int state = SDL_GetAtomicInt(&chunk->mesh_state);
    if (state == JOB_STATE_RUNNING)
    {
        if (SDL_CompareAndSwapAtomicInt(&chunk->mesh_reschedule_pending, 0, 1))
        {
            SDL_AddAtomicInt(&chunk->mesh_epoch, 1);
        }
        return;
    }
    if (state == JOB_STATE_REQUESTED)
    {
        return;
    }

    SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
    SDL_AddAtomicInt(&chunk->mesh_epoch, 1);
}

void request_chunk_light_regen(chunk_t* chunk, int flags)
{
    SDL_SetAtomicInt(&chunk->light_dirty_flags, SDL_GetAtomicInt(&chunk->light_dirty_flags) | flags);

    const int state = SDL_GetAtomicInt(&chunk->light_state);
    if (state == JOB_STATE_RUNNING)
    {
        if (SDL_CompareAndSwapAtomicInt(&chunk->light_reschedule_pending, 0, 1))
        {
            SDL_AddAtomicInt(&chunk->light_epoch, 1);
        }
        return;
    }
    if (state == JOB_STATE_REQUESTED)
    {
        return;
    }

    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    SDL_AddAtomicInt(&chunk->light_epoch, 1);
}

} // namespace

void world_regen_chunk_meshes(int x, int z)
{
    CHECK(!world_chunk_is_border_index(x, z));
    chunk_t* chunk = world_get_chunk_internal(x, z);
    if (!chunk)
    {
        return;
    }
    request_chunk_mesh_regen(chunk);
}

void world_regen_all_chunk_meshes(void)
{
    for (int x = 1; x < MAX_WORLD_WIDTH - 1; ++x)
    for (int z = 1; z < MAX_WORLD_WIDTH - 1; ++z)
    {
        chunk_t* chunk = world_get_chunk_internal(x, z);
        if (!chunk)
        {
            continue;
        }
        request_chunk_mesh_regen(chunk);
    }
}

void world_init(SDL_GPUDevice* handle)
{
    OCT_PROFILE_ZONE("world_init");
    const Uint64 total_start = oct_profile_now_ticks();
    world_set_origin_internal(0, 0);
    Uint64 step_start = oct_profile_now_ticks();
    world_runtime_buffers_init_internal(handle);
    world_runtime_render_pools_init_internal(handle);
    oct_profile_log_duration("Startup timing", "world_init | buffer setup", step_start);
    step_start = oct_profile_now_ticks();
    world_jobs_init(handle);
    oct_profile_log_duration("Startup timing", "world_init | world_jobs_init", step_start);
    step_start = oct_profile_now_ticks();
    for (int x = 0; x < MAX_WORLD_WIDTH; x++)
    for (int z = 0; z < MAX_WORLD_WIDTH; z++)
    {
        world_set_chunk_internal(x, z, world_create_chunk(handle));
    }
    oct_log_infof("Startup timing | world_init allocated %d chunk objects in %.2f ms", MAX_WORLD_WIDTH * MAX_WORLD_WIDTH,
                  oct_profile_elapsed_ms(step_start));
    step_start = oct_profile_now_ticks();
    world_window_sort_chunks_internal();
    oct_profile_log_duration("Startup timing", "world_init | sort_chunks", step_start);
    step_start = oct_profile_now_ticks();
    world_runtime_gen_empty_skylight_internal();
    oct_profile_log_duration("Startup timing", "world_init | gen_skylight", step_start);
    oct_profile_log_duration("Startup timing", "world_init total", total_start);
}

void world_free(void)
{
    world_jobs_free();
    for (int x = 0; x < MAX_WORLD_WIDTH; x++)
    for (int z = 0; z < MAX_WORLD_WIDTH; z++)
    {
        chunk_t* chunk = world_get_chunk_slot_internal(x, z);
        if (chunk)
        {
            world_free_chunk(chunk);
        }
    }
    world_runtime_render_pools_free_internal();
    world_runtime_buffers_free_internal();
}

int world_get_render_distance(void)
{
    return world_active_world_width_internal() - 2;
}

int world_get_generation_worker_count(void)
{
    return world_jobs_get_worker_count();
}

int world_get_generation_worker_limit(void)
{
    return world_jobs_get_worker_limit();
}

void world_set_generation_worker_count(int count)
{
    world_jobs_set_worker_count(count);
}

void world_request_chunk_light_regen_neighborhood_flags(int chunk_x, int chunk_z, int flags)
{
    chunk_t* neighborhood[3][3] = {};
    world_get_neighborhood_internal(chunk_x, chunk_z, neighborhood);
    for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
    {
        chunk_t* chunk = neighborhood[i][j];
        request_chunk_light_regen(chunk, flags);
        world_mark_chunk_urgent(chunk_x + i - 1, chunk_z + j - 1);
    }
}

void world_request_chunk_light_regen_flags(int chunk_x, int chunk_z, int flags)
{
    chunk_t* chunk = world_get_chunk_internal(chunk_x, chunk_z);
    if (chunk)
    {
        request_chunk_light_regen(chunk, flags);
        world_mark_chunk_urgent(chunk_x, chunk_z);
    }
}

void world_request_chunk_light_regen_neighborhood(int chunk_x, int chunk_z)
{
    world_request_chunk_light_regen_neighborhood_flags(chunk_x, chunk_z, LIGHT_DIRTY_ALL);
}

void world_request_chunk_light_regen(int chunk_x, int chunk_z)
{
    world_request_chunk_light_regen_flags(chunk_x, chunk_z, LIGHT_DIRTY_ALL);
}
