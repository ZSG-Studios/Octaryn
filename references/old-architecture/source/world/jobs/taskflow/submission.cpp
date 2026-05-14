#include "world/jobs/taskflow/internal.h"

#include <atomic>

#include "core/check.h"
#include "core/env.h"
#include "core/log.h"
#include "core/profile.h"
#include "world/runtime/world.h"

namespace world_jobs_taskflow_detail {
namespace {

auto slow_submit_logs() -> std::atomic_int&
{
    static std::atomic_int g_slow_submit_logs{0};
    return g_slow_submit_logs;
}

bool job_submit_profile_logging_enabled()
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
    return SDL_clamp(SDL_atoi(value), minimum, maximum);
}

int reserved_urgent_slots()
{
    const int worker_count = active_worker_count();
    if (worker_count <= 1)
    {
        return 0;
    }
    return 1;
}

int reserved_render_slots()
{
    return active_worker_count() >= 3 ? 1 : 0;
}

int cpu_job_limit()
{
    const int worker_count = active_worker_count();
    if (worker_count <= 2)
    {
        return 1;
    }
    const int automatic_limit = SDL_max(2, SDL_min(worker_count - 3, 6));
    const char* value = SDL_getenv("OCTARYN_WORLD_CPU_JOBS");
    if (value && value[0] != '\0')
    {
        return SDL_clamp(SDL_atoi(value), 1, worker_count);
    }
    if (world_edit_recent_changes(250000000ull))
    {
        const int edit_limit = env_int_clamped("OCTARYN_WORLD_EDIT_CPU_JOBS", 3, 1, worker_count);
        return SDL_clamp(SDL_min(automatic_limit, edit_limit), 1, worker_count);
    }
    return SDL_clamp(automatic_limit, 1, worker_count);
}

int regular_slot_limit()
{
    return SDL_max(1, cpu_job_limit() - reserved_urgent_slots());
}

int urgent_slot_limit()
{
    return SDL_max(1, SDL_min(active_worker_count() - reserved_render_slots(), cpu_job_limit() + 1));
}

bool physical_slot_available()
{
    return busy_slot_count().load(std::memory_order_acquire) < static_cast<int>(kTaskflowSlotCount);
}

auto get_job_state(chunk_t* chunk, job_type_t type) -> SDL_AtomicInt*
{
    switch (type)
    {
    case JOB_TYPE_BLOCKS:
        return &chunk->block_state;
    case JOB_TYPE_MESHES:
        return &chunk->mesh_state;
    case JOB_TYPE_LIGHTS:
        return &chunk->light_state;
    default:
        return nullptr;
    }
}

void requeue_unsubmitted_job(taskflow_slot* slot)
{
    if (!slot || !slot->chunk)
    {
        return;
    }

    SDL_AtomicInt* state = get_job_state(slot->chunk, slot->job.type);
    if (state && SDL_GetAtomicInt(state) == JOB_STATE_RUNNING)
    {
        SDL_SetAtomicInt(state, JOB_STATE_REQUESTED);
    }
    if (slot->job.type == JOB_TYPE_LIGHTS)
    {
        SDL_SetAtomicInt(&slot->chunk->light_dirty_flags,
                         SDL_GetAtomicInt(&slot->chunk->light_dirty_flags) |
                             static_cast<int>(slot->light_dirty_flags));
    }
}

} // namespace

bool try_submit_job(int x, int z, job_type_t type)
{
    const Uint64 submit_start = oct_profile_now_ticks();
    chunk_t* chunk = world_get_chunk_internal(x, z);
    CHECK(chunk);

    const bool urgent = SDL_GetAtomicInt(&chunk->urgent_priority) != 0;
    const int slot_limit = urgent ? urgent_slot_limit() : regular_slot_limit();
    if (!physical_slot_available() || cpu_slot_count() >= slot_limit)
    {
        return false;
    }
    taskflow_slot* slot = try_claim_slot(slot_limit);
    if (!slot)
    {
        return false;
    }

    SDL_SetAtomicInt(get_job_state(chunk, type), JOB_STATE_RUNNING);
    slot->chunk = chunk;
    slot->job = job_t{type, x, z};
    slot->epoch = type == JOB_TYPE_MESHES ? SDL_GetAtomicInt(&chunk->mesh_epoch)
                                          : type == JOB_TYPE_LIGHTS ? SDL_GetAtomicInt(&chunk->light_epoch)
                                                                    : 0;
    if (type == JOB_TYPE_LIGHTS)
    {
        slot->light_dirty_flags = static_cast<Uint32>(SDL_GetAtomicInt(&chunk->light_dirty_flags));
        if (slot->light_dirty_flags == 0)
        {
            slot->light_dirty_flags = WORLD_LIGHT_DIRTY_ALL;
        }
        SDL_SetAtomicInt(&chunk->light_dirty_flags, 0);
    }
    else
    {
        slot->light_dirty_flags = WORLD_LIGHT_DIRTY_ALL;
    }

    cpu_buffer_clear(&slot->skylight);
    for (int i = 0; i < MESH_TYPE_COUNT; ++i)
    {
        cpu_buffer_clear(&slot->voxels[i]);
    }

    const Uint64 async_start = oct_profile_now_ticks();
    if (!enqueue_slot(slot))
    {
        requeue_unsubmitted_job(slot);
        release_slot(*slot);
        return false;
    }
    const float async_ms = oct_profile_elapsed_ms(async_start);
    const float submit_ms = oct_profile_elapsed_ms(submit_start);
    if (job_submit_profile_logging_enabled() && submit_ms >= 1.0f)
    {
        const int log_index = slow_submit_logs().fetch_add(1);
        if (log_index < 24)
        {
            oct_log_infof("World job submit timing | type=%d local=(%d,%d) total=%.2fms async=%.2fms active_workers=%d cpu_slots=%d ready_uploads=%d",
                          static_cast<int>(type),
                          x,
                          z,
                          submit_ms,
                          async_ms,
                          active_worker_count(),
                          cpu_slot_count(),
                          ready_upload_count());
        }
    }
    return true;
}

} // namespace world_jobs_taskflow_detail

bool world_jobs_taskflow_regular_slot_available(void)
{
    using namespace world_jobs_taskflow_detail;
    return physical_slot_available() && cpu_slot_count() < regular_slot_limit();
}
