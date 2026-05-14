#include "world/chunks/lifecycle.h"

#include "core/check.h"
#include "world/runtime/internal.h"

chunk_t* world_create_chunk(SDL_GPUDevice* device)
{
    chunk_t* chunk = static_cast<chunk_t*>(SDL_malloc(sizeof(chunk_t)));
    if (!chunk)
    {
        SDL_Log("Failed to allocate chunk");
        return nullptr;
    }
    SDL_zerop(chunk);
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
    SDL_SetAtomicInt(&chunk->mesh_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->light_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->light_dirty_flags, LIGHT_DIRTY_ALL);
    SDL_SetAtomicInt(&chunk->mesh_epoch, 1);
    SDL_SetAtomicInt(&chunk->last_uploaded_mesh_epoch, 0);
    SDL_SetAtomicInt(&chunk->light_epoch, 1);
    SDL_SetAtomicInt(&chunk->last_uploaded_light_epoch, 0);
    SDL_SetAtomicInt(&chunk->render_mesh_valid, 0);
    SDL_SetAtomicInt(&chunk->urgent_priority, 0);
    chunk->data_mutex = SDL_CreateMutex();
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_init(&chunk->gpu_meshes[i], device,
                        SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    }
    gpu_buffer_init(&chunk->gpu_skylight, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    return chunk;
}

void world_free_chunk(chunk_t* chunk)
{
    SDL_DestroyMutex(chunk->data_mutex);
    SDL_free(chunk->blocks);
    gpu_buffer_free(&chunk->gpu_skylight);
    for (int i = 0; i < MESH_TYPE_COUNT; i++)
    {
        gpu_buffer_free(&chunk->gpu_meshes[i]);
    }
    SDL_free(chunk);
}

void world_set_chunk_block_function(void* userdata, int bx, int by, int bz, block_t block)
{
    chunk_t* chunk = static_cast<chunk_t*>(userdata);
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    world_chunk_seed_block(chunk, bx, by, bz, block);
}

void world_seed_chunk_block_direct_function(void* userdata, int bx, int by, int bz, block_t block)
{
    chunk_t* chunk = static_cast<chunk_t*>(userdata);
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    world_chunk_to_local(chunk, &bx, &by, &bz);
    world_chunk_write_local_block(chunk, bx, by, bz, block);
}
