#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include <SDL3/SDL.h>

#include "render/buffer/buffer.h"
#include "runtime/jobs/runtime_worker_pool.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

namespace world_jobs_taskflow_detail {

constexpr std::size_t kTaskflowSlotCount = static_cast<std::size_t>(OCTARYN_WORLD_JOBS_MAX_WORKERS) * 2u;

struct taskflow_slot
{
    std::atomic_bool busy{false};
    std::atomic_bool queued_for_cpu{false};
    std::atomic_bool ready_to_upload{false};
    chunk_t* chunk = nullptr;
    job_t job{};
    int epoch = 0;
    Uint32 light_dirty_flags = WORLD_LIGHT_DIRTY_ALL;
    cpu_buffer_t voxels[MESH_TYPE_COUNT]{};
    cpu_buffer_t skylight{};
};

auto slots() -> std::array<taskflow_slot, kTaskflowSlotCount>&;
auto busy_slot_count() -> std::atomic_int&;
int cpu_slot_count();
int ready_upload_count();
auto worker_pool() -> runtime_worker_pool_t&;
int requested_worker_count();
int active_worker_count();
int active_world_worker_submissions();
Uint64 last_worker_rebuild_ticks();
void request_worker_count(int worker_count);

void init_slots(SDL_GPUDevice* device);
void free_slots();
auto try_claim_slot(int slot_limit) -> taskflow_slot*;
void release_slot(taskflow_slot& slot);
auto clamp_worker_count(int count) -> int;
void rebuild_executor(int worker_count);
void update_worker_policy(int backlog, int pressure);
auto worker_pool_telemetry(int backlog, int pressure) -> runtime_worker_pool_telemetry_t;
void start_workers(int worker_count);
void stop_workers();
bool enqueue_slot(taskflow_slot* slot);
auto execute_job_cpu_only(const job_t* job,
                          chunk_t* chunk,
                          Uint32 light_dirty_flags,
                          cpu_buffer_t voxels[MESH_TYPE_COUNT],
                          cpu_buffer_t* skylight) -> bool;
void commit_ready_uploads(bool drain_all);
bool try_submit_job(int x, int z, job_type_t type);

} // namespace world_jobs_taskflow_detail
