#include "world/jobs/taskflow/internal.h"

#include <atomic>
#include <array>
#include <climits>
#include <cstdint>

#include "core/check.h"
#include "core/env.h"
#include "core/log.h"
#include "core/profile.h"
#include "world/chunks/upload_telemetry.h"

namespace world_jobs_taskflow_detail {
namespace {

auto light_profile_logs() -> std::atomic_int&
{
    static std::atomic_int g_light_profile_logs{0};
    return g_light_profile_logs;
}

bool world_job_profile_logging_enabled()
{
    return oct_env_flag_enabled("OCTARYN_LOG_WORLD_TIMING") || oct_env_flag_enabled("OCTARYN_LOG_RENDER_PROFILE");
}

int env_int_clamped(const char* name, int fallback, int minimum, int maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }

    const int parsed = SDL_atoi(value);
    return SDL_clamp(parsed, minimum, maximum);
}

int max_mesh_uploads_per_pass()
{
    static const int value = env_int_clamped("OCTARYN_MAX_MESH_UPLOADS_PER_FRAME", 4, 0, 16);
    return value;
}

int max_light_uploads_per_pass()
{
    static const int value = env_int_clamped("OCTARYN_MAX_LIGHT_UPLOADS_PER_FRAME", 4, 0, 16);
    return value;
}

Uint64 upload_budget_ns()
{
    static const int budget_us = env_int_clamped("OCTARYN_UPLOAD_BUDGET_US", 1000, 100, 12000);
    return static_cast<Uint64>(budget_us) * 1000ull;
}

Uint64 upload_byte_budget()
{
    static const int budget_kib = env_int_clamped("OCTARYN_UPLOAD_BUDGET_KIB", 1024, 256, 32768);
    return static_cast<Uint64>(budget_kib) * 1024ull;
}

void requeue_stale_mesh_completion(chunk_t* chunk)
{
    world_upload_telemetry_record_mesh_stale();
    if (chunk && SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING)
    {
        SDL_SetAtomicInt(&chunk->mesh_reschedule_pending, 0);
        SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
    }
}

void requeue_stale_light_completion(chunk_t* chunk, Uint32 dirty_flags)
{
    world_upload_telemetry_record_light_stale();
    if (!chunk)
    {
        return;
    }

    if (SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING)
    {
        SDL_SetAtomicInt(&chunk->light_reschedule_pending, 0);
        SDL_SetAtomicInt(&chunk->light_dirty_flags,
                         SDL_GetAtomicInt(&chunk->light_dirty_flags) | static_cast<int>(dirty_flags));
        SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    }
}

struct prepared_mesh_upload
{
    taskflow_slot* slot = nullptr;
    Uint32 counts[MESH_TYPE_COUNT] = {};
    Uint64 bytes = 0;
};

struct prepared_skylight_upload
{
    taskflow_slot* slot = nullptr;
    Uint32 skylight_byte_count = 0;
    Uint64 bytes = 0;
};

struct prepared_upload_batch
{
    std::array<prepared_mesh_upload, kTaskflowSlotCount> meshes{};
    std::array<prepared_skylight_upload, kTaskflowSlotCount> skylights{};
    std::size_t mesh_count = 0;
    std::size_t skylight_count = 0;
    Uint64 bytes = 0;
    gpu_buffer_t* first_upload = nullptr;
};

void note_first_upload(prepared_upload_batch* batch, gpu_buffer_t* buffer)
{
    if (!batch->first_upload && buffer)
    {
        batch->first_upload = buffer;
    }
}

bool upload_fits_batch(const prepared_upload_batch* batch, Uint64 bytes, Uint64 byte_budget)
{
    return batch->bytes == 0 || (batch->bytes + bytes) <= byte_budget;
}

void fail_prepared_uploads(prepared_upload_batch* batch)
{
    for (std::size_t i = 0; i < batch->mesh_count; ++i)
    {
        world_fail_chunk_meshes_upload_internal(batch->meshes[i].slot->chunk, batch->meshes[i].bytes);
    }
    for (std::size_t i = 0; i < batch->skylight_count; ++i)
    {
        taskflow_slot* slot = batch->skylights[i].slot;
        world_fail_chunk_lights_upload_internal(slot->chunk, slot->light_dirty_flags, batch->skylights[i].bytes);
    }
}

void release_prepared_uploads(prepared_upload_batch* batch)
{
    for (std::size_t i = 0; i < batch->mesh_count; ++i)
    {
        release_slot(*batch->meshes[i].slot);
    }
    for (std::size_t i = 0; i < batch->skylight_count; ++i)
    {
        release_slot(*batch->skylights[i].slot);
    }
}

bool stage_prepared_uploads(prepared_upload_batch* batch)
{
    for (std::size_t i = 0; i < batch->mesh_count; ++i)
    {
        taskflow_slot* slot = batch->meshes[i].slot;
        if (!world_stage_chunk_meshes_upload_internal(slot->chunk, slot->voxels))
        {
            return false;
        }
    }
    for (std::size_t i = 0; i < batch->skylight_count; ++i)
    {
        taskflow_slot* slot = batch->skylights[i].slot;
        if (!world_stage_chunk_lights_upload_internal(slot->chunk,
                                                      &slot->skylight,
                                                      slot->light_dirty_flags))
        {
            return false;
        }
    }
    return true;
}

void commit_prepared_uploads(prepared_upload_batch* batch)
{
    for (std::size_t i = 0; i < batch->mesh_count; ++i)
    {
        taskflow_slot* slot = batch->meshes[i].slot;
        world_commit_chunk_meshes_upload_internal(slot->chunk, batch->meshes[i].counts, batch->meshes[i].bytes);
    }
    for (std::size_t i = 0; i < batch->skylight_count; ++i)
    {
        taskflow_slot* slot = batch->skylights[i].slot;
        world_commit_chunk_lights_upload_internal(slot->chunk,
                                                 batch->skylights[i].skylight_byte_count,
                                                 slot->light_dirty_flags,
                                                 batch->skylights[i].bytes);
    }
}

} // namespace

auto execute_job_cpu_only(const job_t* job,
                          chunk_t* chunk,
                          Uint32 light_dirty_flags,
                          cpu_buffer_t voxels[MESH_TYPE_COUNT],
                          cpu_buffer_t* skylight) -> bool
{
    cpu_buffer_clear(skylight);
    for (int i = 0; i < MESH_TYPE_COUNT; ++i)
    {
        cpu_buffer_clear(&voxels[i]);
    }

    if (!chunk || chunk != world_get_chunk_internal(job->x, job->z))
    {
        SDL_Log("Skipped stale world job type=%d local=(%d,%d)", static_cast<int>(job->type), job->x, job->z);
        if (chunk)
        {
            if (job->type == JOB_TYPE_BLOCKS && SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_RUNNING)
            {
                SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
            }
            else if (job->type == JOB_TYPE_MESHES && SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING)
            {
                SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
            }
            else if (job->type == JOB_TYPE_LIGHTS && SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING)
            {
                SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
            }
        }
        return false;
    }
    if (job->type == JOB_TYPE_BLOCKS)
    {
        world_gen_chunk_blocks_internal(chunk);
        return false;
    }

    chunk_t* neighborhood[3][3] = {};
    world_get_neighborhood_internal(job->x, job->z, neighborhood);
    if (job->type == JOB_TYPE_MESHES)
    {
        world_build_chunk_meshes_internal(neighborhood, voxels);
        return true;
    }
    if (job->type == JOB_TYPE_LIGHTS)
    {
        const Uint64 start_ticks = oct_profile_now_ticks();
        world_build_chunk_lights_internal(neighborhood, skylight, light_dirty_flags);
        const float elapsed_ms = oct_profile_elapsed_ms(start_ticks);
        if (world_job_profile_logging_enabled() && elapsed_ms >= 1.5f)
        {
            const int log_index = light_profile_logs().fetch_add(1);
            if (log_index < 24)
            {
                oct_log_infof("Lighting timing | chunk (%d, %d) block=%d skylight=%d took %.2f ms",
                              job->x,
                              job->z,
                              (light_dirty_flags & WORLD_LIGHT_DIRTY_BLOCK) != 0,
                              (light_dirty_flags & WORLD_LIGHT_DIRTY_SKYLIGHT) != 0,
                              elapsed_ms);
            }
        }
        return true;
    }

    CHECK(false);
    return false;
}

void commit_ready_uploads(bool drain_all)
{
    const int max_mesh_uploads = drain_all ? INT_MAX : max_mesh_uploads_per_pass();
    const int max_light_uploads = drain_all ? INT_MAX : max_light_uploads_per_pass();
    const Uint64 budget_ns = drain_all ? UINT64_MAX : upload_budget_ns();
    const Uint64 byte_budget = drain_all ? UINT64_MAX : upload_byte_budget();
    const Uint64 upload_budget_start = SDL_GetTicksNS();
    int mesh_uploads = 0;
    int light_uploads = 0;
    prepared_upload_batch batch;
    for (taskflow_slot& slot : slots())
    {
        if ((SDL_GetTicksNS() - upload_budget_start) >= budget_ns)
        {
            break;
        }
        if (!slot.busy.load(std::memory_order_acquire))
        {
            continue;
        }
        if (!slot.ready_to_upload.exchange(false, std::memory_order_acq_rel))
        {
            continue;
        }

        chunk_t* chunk = slot.chunk;
        CHECK(chunk);
        switch (slot.job.type)
        {
        case JOB_TYPE_MESHES:
            if (mesh_uploads >= max_mesh_uploads)
            {
                world_upload_telemetry_record_mesh_deferred();
                slot.ready_to_upload.store(true, std::memory_order_release);
                continue;
            }
            if (slot.epoch != SDL_GetAtomicInt(&chunk->mesh_epoch))
            {
                requeue_stale_mesh_completion(chunk);
                release_slot(slot);
                continue;
            }
            {
                prepared_mesh_upload& upload = batch.meshes[batch.mesh_count];
                upload.slot = &slot;
                if (!world_prepare_chunk_meshes_upload_internal(chunk, slot.voxels, upload.counts, &upload.bytes))
                {
                    release_slot(slot);
                    continue;
                }
                if (!upload_fits_batch(&batch, upload.bytes, byte_budget))
                {
                    world_upload_telemetry_record_mesh_deferred();
                    slot.ready_to_upload.store(true, std::memory_order_release);
                    continue;
                }
                note_first_upload(&batch, world_first_chunk_mesh_upload_buffer_internal(chunk, slot.voxels));
                batch.bytes += upload.bytes;
                ++batch.mesh_count;
            }
            ++mesh_uploads;
            break;
        case JOB_TYPE_LIGHTS:
            if (light_uploads >= max_light_uploads)
            {
                world_upload_telemetry_record_light_deferred();
                slot.ready_to_upload.store(true, std::memory_order_release);
                continue;
            }
            if (slot.epoch != SDL_GetAtomicInt(&chunk->light_epoch))
            {
                requeue_stale_light_completion(chunk, slot.light_dirty_flags);
                release_slot(slot);
                continue;
            }
            {
                prepared_skylight_upload& upload = batch.skylights[batch.skylight_count];
                upload.slot = &slot;
                if (!world_prepare_chunk_lights_upload_internal(chunk,
                                                                &slot.skylight,
                                                                slot.light_dirty_flags,
                                                                &upload.skylight_byte_count,
                                                                &upload.bytes))
                {
                    release_slot(slot);
                    continue;
                }
                if (!upload_fits_batch(&batch, upload.bytes, byte_budget))
                {
                    world_upload_telemetry_record_light_deferred();
                    slot.ready_to_upload.store(true, std::memory_order_release);
                    continue;
                }
                note_first_upload(&batch,
                                  world_first_chunk_light_upload_buffer_internal(chunk,
                                                                                &slot.skylight,
                                                                                slot.light_dirty_flags));
                batch.bytes += upload.bytes;
                ++batch.skylight_count;
            }
            ++light_uploads;
            break;
        default:
            CHECK(false);
            break;
        }

        if (!drain_all && (SDL_GetTicksNS() - upload_budget_start) >= budget_ns)
        {
            break;
        }
    }

    if (batch.mesh_count == 0 && batch.skylight_count == 0)
    {
        return;
    }

    const bool needs_submit = batch.first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(batch.first_upload))
    {
        fail_prepared_uploads(&batch);
        release_prepared_uploads(&batch);
        return;
    }
    if (!stage_prepared_uploads(&batch))
    {
        if (needs_submit)
        {
            gpu_buffer_abort_upload(batch.first_upload);
        }
        fail_prepared_uploads(&batch);
        release_prepared_uploads(&batch);
        return;
    }
    if (needs_submit && !gpu_buffer_end_upload(batch.first_upload))
    {
        fail_prepared_uploads(&batch);
        release_prepared_uploads(&batch);
        return;
    }

    commit_prepared_uploads(&batch);
    release_prepared_uploads(&batch);
}

} // namespace world_jobs_taskflow_detail
