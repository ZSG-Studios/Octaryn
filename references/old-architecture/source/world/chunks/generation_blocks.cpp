#include <SDL3/SDL.h>

#include "core/check.h"
#include "core/persistence/persistence.h"
#include "world/chunks/lifecycle.h"
#include "world/runtime/private.h"
#include "world/generation/worldgen.h"

void world_chunk_runtime_generate_blocks(chunk_t* chunk)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    CHECK(SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_REQUESTED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_REQUESTED);
    const bool has_blocks = world_chunk_ensure_blocks_allocated(chunk);
    CHECK(has_blocks);
    if (!has_blocks)
    {
        return;
    }

    world_chunk_clear_blocks(chunk);
    worldgen_get_blocks(chunk, chunk->position[0], chunk->position[1], world_seed_chunk_block_direct_function);
    persistence_get_blocks(chunk, chunk->position[0], chunk->position[1], world_seed_chunk_block_direct_function);
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING);
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_COMPLETED);
    if (SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_REQUESTED)
    {
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    }
}
