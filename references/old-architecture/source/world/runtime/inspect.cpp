#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "world/runtime/world.h"
#include "world/chunks/state.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

static bool chunk_is_visible_to_camera(const camera_t* camera, const chunk_t* chunk)
{
    return camera_get_vis(camera,
                          static_cast<float>(world_chunk_world_x(chunk)),
                          0.0f,
                          static_cast<float>(world_chunk_world_z(chunk)),
                          static_cast<float>(CHUNK_WIDTH),
                          static_cast<float>(CHUNK_HEIGHT),
                          static_cast<float>(CHUNK_WIDTH));
}

void world_get_debug_stats(world_debug_stats_t* out_stats)
{
    if (!out_stats)
    {
        return;
    }

    const int active_world_width = world_active_world_width_internal();
    *out_stats = {};
    out_stats->active_chunks = active_world_width * active_world_width;
    out_stats->running_jobs = world_jobs_running_count();
    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        chunk_t* chunk = world_get_chunk_internal(x, z);
        if (!chunk)
        {
            continue;
        }
        if (SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED)
        {
            out_stats->loaded_chunks++;
        }
        if (SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_COMPLETED)
        {
            out_stats->mesh_ready_chunks++;
        }
        if (chunk->gpu_meshes[MESH_TYPE_OPAQUE].size > 0)
        {
            out_stats->chunks_with_opaque_mesh++;
            out_stats->opaque_vertices += chunk->gpu_meshes[MESH_TYPE_OPAQUE].size;
        }
        else if (chunk->pooled_face_counts[MESH_TYPE_OPAQUE] > 0)
        {
            out_stats->chunks_with_opaque_mesh++;
            out_stats->opaque_vertices += chunk->pooled_face_counts[MESH_TYPE_OPAQUE] * 4u;
        }
        if (chunk->gpu_meshes[MESH_TYPE_TRANSPARENT].size > 0)
        {
            out_stats->chunks_with_transparent_mesh++;
            out_stats->transparent_vertices += chunk->gpu_meshes[MESH_TYPE_TRANSPARENT].size;
        }
        else if (chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT] > 0)
        {
            out_stats->chunks_with_transparent_mesh++;
            out_stats->transparent_vertices += chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT] * 4u;
        }
        if (chunk->gpu_meshes[MESH_TYPE_SPRITE].size > 0)
        {
            out_stats->chunks_with_sprite_mesh++;
            out_stats->sprite_vertices += chunk->gpu_meshes[MESH_TYPE_SPRITE].size;
        }
        out_stats->opaque_faces += chunk->pooled_face_counts[MESH_TYPE_OPAQUE];
        out_stats->transparent_faces += chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT];
        out_stats->cube_faces += chunk->pooled_face_counts[MESH_TYPE_OPAQUE] +
                                 chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT];
    }
    out_stats->dirty_pooled_chunk_slots = world_runtime_dirty_pooled_chunk_slot_count_internal();
}

int world_count_visible_mesh_chunks(const camera_t* camera)
{
    const int active_world_width = world_active_world_width_internal();
    int visible_chunks = 0;
    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        if (world_chunk_is_border_index(x, z))
        {
            continue;
        }
        chunk_t* chunk = world_get_chunk_internal(x, z);
        if (!world_chunk_render_ready(chunk))
        {
            continue;
        }
        if (chunk_is_visible_to_camera(camera, chunk))
        {
            visible_chunks++;
        }
    }
    return visible_chunks;
}

int world_count_visible_chunks_missing_skylight(const camera_t* camera)
{
    const int active_world_width = world_active_world_width_internal();
    int visible_missing_chunks = 0;
    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        if (world_chunk_is_border_index(x, z))
        {
            continue;
        }
        chunk_t* chunk = world_get_chunk_internal(x, z);
        if (!world_chunk_render_ready(chunk))
        {
            continue;
        }
        if (!chunk_is_visible_to_camera(camera, chunk))
        {
            continue;
        }
        if (!world_chunk_light_ready(chunk))
        {
            visible_missing_chunks++;
        }
    }
    return visible_missing_chunks;
}
