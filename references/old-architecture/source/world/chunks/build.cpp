#include <SDL3/SDL.h>

#include "core/check.h"
#include "core/profile.h"
#include "world/chunks/build_light.h"
#include "world/chunks/build_mesh.h"
#include "world/chunks/snapshot.h"
#include "world/chunks/state.h"

void world_build_chunk_meshes_internal(chunk_t* neighbor_chunks[3][3], cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    OCT_PROFILE_ZONE("world_build_chunk_meshes_internal");
    chunk_t* chunk = neighbor_chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING);
    const neighborhood_snapshot_t snapshot = world_capture_neighborhood_snapshot(neighbor_chunks, true);
    world_chunk_build_meshes(snapshot, meshes);
}

void world_build_chunk_lights_internal(chunk_t* neighbor_chunks[3][3], cpu_buffer_t* skylight, Uint32 dirty_flags)
{
    OCT_PROFILE_ZONE("world_build_chunk_lights_internal");
    chunk_t* chunk = neighbor_chunks[1][1];
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(world_chunk_light_job_active(chunk));
    const neighborhood_snapshot_t snapshot = world_capture_neighborhood_snapshot(neighbor_chunks, false);
    world_chunk_build_lights(snapshot, skylight, dirty_flags);
}
