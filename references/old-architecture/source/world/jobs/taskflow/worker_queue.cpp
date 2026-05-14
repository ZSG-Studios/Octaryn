#include "world/jobs/taskflow/internal.h"

#include <array>
#include <atomic>
#include <deque>

#include "core/profile.h"

namespace world_jobs_taskflow_detail {
namespace {

struct worker_queue_t
{
    std::array<SDL_Thread*, OCTARYN_WORLD_JOBS_MAX_WORKERS> threads{};
    SDL_Mutex* mutex = nullptr;
    SDL_Condition* condition = nullptr;
    std::deque<taskflow_slot*> urgent_slots{};
    std::deque<taskflow_slot*> regular_slots{};
    std::atomic_bool stopping{false};
    std::atomic_int requested_workers{0};
    std::atomic_int active_workers{0};
    std::atomic_int active_submissions{0};
    float last_rebuild_ms = 0.0f;
    Uint64 last_rebuild_ticks = 0;
};

auto queue_state() -> worker_queue_t&
{
    static worker_queue_t g_queue{};
    return g_queue;
}

void ensure_queue_sync(worker_queue_t* queue)
{
    if (!queue->mutex)
    {
        queue->mutex = SDL_CreateMutex();
    }
    if (!queue->condition)
    {
        queue->condition = SDL_CreateCondition();
    }
}

void execute_slot(taskflow_slot* slot)
{
    if (!slot)
    {
        return;
    }

    OCT_PROFILE_ZONE("world_job_execute_worker_queue");
    const bool requires_upload =
        execute_job_cpu_only(&slot->job,
                             slot->chunk,
                             slot->light_dirty_flags,
                             slot->voxels,
                             &slot->skylight);
    if (requires_upload)
    {
        slot->ready_to_upload.store(true, std::memory_order_release);
        return;
    }
    release_slot(*slot);
}

auto pop_slot(worker_queue_t* queue) -> taskflow_slot*
{
    ensure_queue_sync(queue);
    SDL_LockMutex(queue->mutex);
    while (!queue->stopping.load(std::memory_order_acquire) &&
           queue->urgent_slots.empty() &&
           queue->regular_slots.empty())
    {
        SDL_WaitCondition(queue->condition, queue->mutex);
    }

    taskflow_slot* slot = nullptr;
    if (!queue->urgent_slots.empty())
    {
        slot = queue->urgent_slots.front();
        queue->urgent_slots.pop_front();
    }
    else if (!queue->regular_slots.empty())
    {
        slot = queue->regular_slots.front();
        queue->regular_slots.pop_front();
    }
    SDL_UnlockMutex(queue->mutex);

    if (slot)
    {
        slot->queued_for_cpu.store(false, std::memory_order_release);
    }
    return slot;
}

int worker_entry(void* data)
{
    worker_queue_t* queue = static_cast<worker_queue_t*>(data);
    while (!queue->stopping.load(std::memory_order_acquire))
    {
        taskflow_slot* slot = pop_slot(queue);
        if (!slot)
        {
            continue;
        }

        queue->active_submissions.fetch_add(1, std::memory_order_acq_rel);
        execute_slot(slot);
        queue->active_submissions.fetch_sub(1, std::memory_order_acq_rel);
    }
    return 0;
}

} // namespace

void start_workers(int worker_count)
{
    worker_queue_t& queue = queue_state();
    stop_workers();

    const Uint64 rebuild_start = oct_profile_now_ticks();
    ensure_queue_sync(&queue);
    SDL_LockMutex(queue.mutex);
    queue.urgent_slots.clear();
    queue.regular_slots.clear();
    SDL_UnlockMutex(queue.mutex);
    queue.stopping.store(false, std::memory_order_release);
    queue.active_submissions.store(0, std::memory_order_release);
    queue.requested_workers.store(worker_count, std::memory_order_release);

    int started = 0;
    for (int i = 0; i < worker_count; ++i)
    {
        const std::size_t index = static_cast<std::size_t>(i);
        queue.threads[index] = SDL_CreateThread(worker_entry, "oct-world-worker", &queue);
        if (queue.threads[index])
        {
            ++started;
        }
    }
    queue.active_workers.store(started, std::memory_order_release);
    queue.last_rebuild_ms = oct_profile_elapsed_ms(rebuild_start);
    queue.last_rebuild_ticks = oct_profile_now_ticks();
}

void request_worker_count(int worker_count)
{
    queue_state().requested_workers.store(worker_count, std::memory_order_release);
}

void stop_workers()
{
    worker_queue_t& queue = queue_state();
    const int active_workers = queue.active_workers.exchange(0, std::memory_order_acq_rel);
    if (active_workers > 0)
    {
        queue.stopping.store(true, std::memory_order_release);
        if (queue.condition)
        {
            SDL_BroadcastCondition(queue.condition);
        }
        for (int i = 0; i < active_workers; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            if (queue.threads[index])
            {
                SDL_WaitThread(queue.threads[index], nullptr);
                queue.threads[index] = nullptr;
            }
        }
    }

    queue.requested_workers.store(0, std::memory_order_release);
    queue.active_submissions.store(0, std::memory_order_release);
    if (queue.mutex)
    {
        SDL_LockMutex(queue.mutex);
        queue.urgent_slots.clear();
        queue.regular_slots.clear();
        SDL_UnlockMutex(queue.mutex);
    }
}

bool enqueue_slot(taskflow_slot* slot)
{
    worker_queue_t& queue = queue_state();
    if (!slot || queue.active_workers.load(std::memory_order_acquire) <= 0)
    {
        return false;
    }

    ensure_queue_sync(&queue);
    slot->queued_for_cpu.store(true, std::memory_order_release);
    SDL_LockMutex(queue.mutex);
    if (slot->chunk && SDL_GetAtomicInt(&slot->chunk->urgent_priority) != 0)
    {
        queue.urgent_slots.push_back(slot);
    }
    else
    {
        queue.regular_slots.push_back(slot);
    }
    SDL_SignalCondition(queue.condition);
    SDL_UnlockMutex(queue.mutex);
    return true;
}

auto worker_pool() -> runtime_worker_pool_t&
{
    return runtime_worker_pool_shared();
}

int requested_worker_count()
{
    const worker_queue_t& queue = queue_state();
    return queue.requested_workers.load(std::memory_order_acquire);
}

int active_worker_count()
{
    const worker_queue_t& queue = queue_state();
    return queue.active_workers.load(std::memory_order_acquire);
}

auto worker_pool_telemetry(int backlog, int pressure) -> runtime_worker_pool_telemetry_t
{
    const worker_queue_t& queue = queue_state();
    return runtime_worker_pool_telemetry_t{
        .requested_workers = queue.requested_workers.load(std::memory_order_acquire),
        .active_workers = queue.active_workers.load(std::memory_order_acquire),
        .rebuild_ms = queue.last_rebuild_ms,
        .backlog = backlog,
        .pressure = pressure,
    };
}

int active_world_worker_submissions()
{
    const worker_queue_t& queue = queue_state();
    return queue.active_submissions.load(std::memory_order_acquire);
}

Uint64 last_worker_rebuild_ticks()
{
    const worker_queue_t& queue = queue_state();
    return queue.last_rebuild_ticks;
}

} // namespace world_jobs_taskflow_detail
