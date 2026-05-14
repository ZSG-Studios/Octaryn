#include <SDL3/SDL.h>

#include "world/runtime/internal.h"
#include "world/runtime/private.h"

bool world_chunk_mesh_ready(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_COMPLETED;
}

bool world_chunk_mesh_running(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING;
}

int world_chunk_mesh_epoch(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->mesh_epoch);
}

int world_chunk_last_uploaded_mesh_epoch(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->last_uploaded_mesh_epoch);
}

int world_chunk_light_epoch(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->light_epoch);
}

int world_chunk_last_uploaded_light_epoch(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->last_uploaded_light_epoch);
}

bool world_chunk_render_mesh_valid(const chunk_t* chunk)
{
    return SDL_GetAtomicInt(const_cast<SDL_AtomicInt*>(&chunk->render_mesh_valid)) != 0;
}
