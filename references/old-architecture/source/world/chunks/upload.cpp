#include <SDL3/SDL.h>

#include "world/chunks/state.h"
#include "world/runtime/internal.h"
#include "world/chunks/upload_light.h"
#include "world/chunks/upload_mesh.h"
#include "world/chunks/upload_telemetry.h"
#include "world/runtime/private.h"

namespace {

bool chunk_blocks_ready_for_upload(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
}

bool chunk_mesh_upload_still_current(chunk_t* chunk)
{
    return chunk_blocks_ready_for_upload(chunk) &&
        SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING;
}

bool chunk_light_upload_still_current(chunk_t* chunk)
{
    return chunk_blocks_ready_for_upload(chunk) &&
        SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING;
}

void requeue_stale_mesh_upload(chunk_t* chunk)
{
    if (SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING)
    {
        SDL_SetAtomicInt(&chunk->mesh_reschedule_pending, 0);
        SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
    }
}

void requeue_stale_light_upload(chunk_t* chunk, Uint32 dirty_flags)
{
    if (SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING)
    {
        SDL_SetAtomicInt(&chunk->light_reschedule_pending, 0);
        SDL_SetAtomicInt(&chunk->light_dirty_flags,
                         SDL_GetAtomicInt(&chunk->light_dirty_flags) | static_cast<int>(dirty_flags));
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    }
}

Uint64 cpu_buffer_byte_size(const cpu_buffer_t* buffer)
{
    if (!buffer)
    {
        return 0;
    }
    return static_cast<Uint64>(buffer->size) * static_cast<Uint64>(buffer->stride);
}

Uint64 mesh_upload_byte_size(cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    Uint64 bytes = 0;
    for (int i = 0; i < MESH_TYPE_COUNT; ++i)
    {
        bytes += cpu_buffer_byte_size(&meshes[i]);
    }
    return bytes;
}

Uint64 light_upload_byte_size(cpu_buffer_t* skylight, Uint32 dirty_flags)
{
    Uint64 bytes = 0;
    if ((dirty_flags & WORLD_LIGHT_DIRTY_SKYLIGHT) != 0)
    {
        bytes += cpu_buffer_byte_size(skylight);
    }
    return bytes;
}

} // namespace

void world_upload_chunk_meshes_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    Uint32 counts[MESH_TYPE_COUNT] = {};
    Uint64 upload_bytes = 0;
    if (!world_prepare_chunk_meshes_upload_internal(chunk, meshes, counts, &upload_bytes))
    {
        return;
    }

    gpu_buffer_t* first_upload = world_first_chunk_mesh_upload_buffer_internal(chunk, meshes);
    const bool needs_submit = first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(first_upload))
    {
        world_fail_chunk_meshes_upload_internal(chunk, upload_bytes);
        return;
    }
    if (!world_stage_chunk_meshes_upload_internal(chunk, meshes))
    {
        if (needs_submit)
        {
            gpu_buffer_abort_upload(first_upload);
        }
        world_fail_chunk_meshes_upload_internal(chunk, upload_bytes);
        return;
    }
    if (needs_submit && !gpu_buffer_end_upload(first_upload))
    {
        world_fail_chunk_meshes_upload_internal(chunk, upload_bytes);
        return;
    }

    world_commit_chunk_meshes_upload_internal(chunk, counts, upload_bytes);
}

bool world_prepare_chunk_meshes_upload_internal(chunk_t* chunk,
                                                cpu_buffer_t meshes[MESH_TYPE_COUNT],
                                                Uint32 counts[MESH_TYPE_COUNT],
                                                Uint64* upload_bytes)
{
    if (!chunk_mesh_upload_still_current(chunk))
    {
        requeue_stale_mesh_upload(chunk);
        world_upload_telemetry_record_mesh_stale();
        return false;
    }

    *upload_bytes = mesh_upload_byte_size(meshes);
    if (!world_chunk_runtime_prepare_meshes(chunk, meshes, counts))
    {
        world_fail_chunk_meshes_upload_internal(chunk, *upload_bytes);
        return false;
    }
    return true;
}

gpu_buffer_t* world_first_chunk_mesh_upload_buffer_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    return world_chunk_runtime_first_mesh_upload_buffer(chunk, meshes);
}

bool world_stage_chunk_meshes_upload_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    return world_chunk_runtime_stage_prepared_meshes(chunk, meshes);
}

void world_commit_chunk_meshes_upload_internal(chunk_t* chunk,
                                               const Uint32 counts[MESH_TYPE_COUNT],
                                               Uint64 upload_bytes)
{
    world_chunk_runtime_commit_meshes(chunk, counts);
    world_upload_telemetry_record_mesh_result(upload_bytes, true);
    SDL_SetAtomicInt(&chunk->last_uploaded_mesh_epoch, SDL_GetAtomicInt(&chunk->mesh_epoch));
    world_note_edit_mesh_upload(chunk);
    world_runtime_sync_chunk_descriptor_internal(chunk);
    SDL_SetAtomicInt(&chunk->render_mesh_valid, 1);
    SDL_SetAtomicInt(&chunk->mesh_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_COMPLETED);
}

void world_fail_chunk_meshes_upload_internal(chunk_t* chunk, Uint64 upload_bytes)
{
    world_upload_telemetry_record_mesh_result(upload_bytes, false);
    if (!world_chunk_has_any_gpu_mesh(chunk))
    {
        SDL_SetAtomicInt(&chunk->render_mesh_valid, 0);
    }
    SDL_SetAtomicInt(&chunk->mesh_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
    SDL_AddAtomicInt(&chunk->mesh_epoch, 1);
}

void world_upload_chunk_lights_internal(chunk_t* chunk, cpu_buffer_t* skylight, Uint32 dirty_flags)
{
    Uint32 skylight_byte_count = 0;
    Uint64 upload_bytes = 0;
    if (!world_prepare_chunk_lights_upload_internal(chunk,
                                                    skylight,
                                                    dirty_flags,
                                                    &skylight_byte_count,
                                                    &upload_bytes))
    {
        return;
    }

    gpu_buffer_t* first_upload = world_first_chunk_light_upload_buffer_internal(chunk, skylight, dirty_flags);
    const bool needs_submit = first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(first_upload))
    {
        world_fail_chunk_lights_upload_internal(chunk, dirty_flags, upload_bytes);
        return;
    }
    if (!world_stage_chunk_lights_upload_internal(chunk, skylight, dirty_flags))
    {
        if (needs_submit)
        {
            gpu_buffer_abort_upload(first_upload);
        }
        world_fail_chunk_lights_upload_internal(chunk, dirty_flags, upload_bytes);
        return;
    }
    if (needs_submit && !gpu_buffer_end_upload(first_upload))
    {
        world_fail_chunk_lights_upload_internal(chunk, dirty_flags, upload_bytes);
        return;
    }

    world_commit_chunk_lights_upload_internal(chunk, skylight_byte_count, dirty_flags, upload_bytes);
}

bool world_prepare_chunk_lights_upload_internal(chunk_t* chunk,
                                                cpu_buffer_t* skylight,
                                                Uint32 dirty_flags,
                                                Uint32* skylight_byte_count,
                                                Uint64* upload_bytes)
{
    if (!chunk_light_upload_still_current(chunk))
    {
        requeue_stale_light_upload(chunk, dirty_flags);
        world_upload_telemetry_record_light_stale();
        return false;
    }

    *upload_bytes = light_upload_byte_size(skylight, dirty_flags);
    if (!world_chunk_runtime_prepare_lights(chunk, skylight, dirty_flags, skylight_byte_count))
    {
        world_fail_chunk_lights_upload_internal(chunk, dirty_flags, *upload_bytes);
        return false;
    }
    return true;
}

gpu_buffer_t* world_first_chunk_light_upload_buffer_internal(chunk_t* chunk,
                                                            cpu_buffer_t* skylight,
                                                            Uint32 dirty_flags)
{
    return world_chunk_runtime_first_light_upload_buffer(chunk, skylight, dirty_flags);
}

bool world_stage_chunk_lights_upload_internal(chunk_t* chunk,
                                              cpu_buffer_t* skylight,
                                              Uint32 dirty_flags)
{
    return world_chunk_runtime_stage_prepared_lights(chunk, skylight, dirty_flags);
}

void world_commit_chunk_lights_upload_internal(chunk_t* chunk,
                                               Uint32 skylight_byte_count,
                                               Uint32 dirty_flags,
                                               Uint64 upload_bytes)
{
    if (!world_chunk_runtime_commit_lights(chunk, skylight_byte_count, dirty_flags))
    {
        world_fail_chunk_lights_upload_internal(chunk, dirty_flags, upload_bytes);
        return;
    }

    world_upload_telemetry_record_light_result(upload_bytes, true);
    SDL_SetAtomicInt(&chunk->last_uploaded_light_epoch, SDL_GetAtomicInt(&chunk->light_epoch));
    world_note_edit_light_upload(chunk, dirty_flags);
    SDL_SetAtomicInt(&chunk->light_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_COMPLETED);
}

void world_fail_chunk_lights_upload_internal(chunk_t* chunk, Uint32 dirty_flags, Uint64 upload_bytes)
{
    world_upload_telemetry_record_light_result(upload_bytes, false);
    SDL_SetAtomicInt(&chunk->light_dirty_flags, SDL_GetAtomicInt(&chunk->light_dirty_flags) | static_cast<int>(dirty_flags));
    SDL_SetAtomicInt(&chunk->light_reschedule_pending, 0);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    SDL_AddAtomicInt(&chunk->light_epoch, 1);
}
