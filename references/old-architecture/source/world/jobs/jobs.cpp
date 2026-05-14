#include <SDL3/SDL.h>

#include "core/profile.h"
#include "world/jobs/scheduling.h"
#include "world/jobs/taskflow/backend.h"
#include "world/runtime/private.h"
#include "world/runtime/update_telemetry.h"

namespace {

constexpr float kWorldJobsSpikeLogThresholdMs = 8.0f;
constexpr Uint64 kWorldJobsSpikeLogIntervalNs = 1000000000ull;
Uint64 g_last_world_jobs_spike_log_ticks = 0;

void log_world_jobs_spike(float total_ms,
                          float pending_uploads_ms,
                          float executor_ms,
                          float urgent_scan_ms,
                          float regular_scan_ms,
                          float urgent_probe_max_ms,
                          float regular_probe_max_ms,
                          int active_chunks,
                          int running_jobs,
                          Uint32 urgent_submissions,
                          Uint32 regular_submissions,
                          runtime_worker_pool_telemetry_t worker_telemetry)
{
    if (total_ms < kWorldJobsSpikeLogThresholdMs)
    {
        return;
    }

    const Uint64 now = oct_profile_now_ticks();
    if (g_last_world_jobs_spike_log_ticks != 0u &&
        now - g_last_world_jobs_spike_log_ticks < kWorldJobsSpikeLogIntervalNs)
    {
        return;
    }
    g_last_world_jobs_spike_log_ticks = now;

    SDL_Log("World jobs spike: total=%.2fms pending_uploads=%.2f executor=%.2f urgent_scan=%.2f regular_scan=%.2f urgent_probe_max=%.2f regular_probe_max=%.2f active_chunks=%d running_jobs=%d urgent_submit=%u regular_submit=%u worker_auto_target=%d worker_active=%d worker_rebuild_ms=%.2f worker_backlog=%d worker_pressure=%d",
            total_ms,
            pending_uploads_ms,
            executor_ms,
            urgent_scan_ms,
            regular_scan_ms,
            urgent_probe_max_ms,
            regular_probe_max_ms,
            active_chunks,
            running_jobs,
            urgent_submissions,
            regular_submissions,
            worker_telemetry.requested_workers,
            worker_telemetry.active_workers,
            worker_telemetry.rebuild_ms,
            worker_telemetry.backlog,
            worker_telemetry.pressure);
}

int worker_backlog_from_report(const world_jobs_schedule_report_t& report)
{
    int backlog = world_jobs_taskflow_cpu_running_count() +
                  static_cast<int>(report.urgent_submissions + report.regular_submissions);
    if (!world_jobs_taskflow_regular_slot_available())
    {
        backlog = SDL_max(backlog, world_jobs_taskflow_active_worker_count());
    }
    return backlog;
}

int worker_pressure_from_uploads(float pending_uploads_ms)
{
    const int waiting_uploads = world_jobs_taskflow_upload_waiting_count();
    const int upload_wait_pressure = waiting_uploads > 0 ? 80 : 0;
    const int upload_time_pressure = pending_uploads_ms >= 0.75f ? 80 : 0;
    return SDL_clamp(upload_wait_pressure + upload_time_pressure, 0, 100);
}

} // namespace

int world_jobs_running_count(void)
{
    return world_jobs_taskflow_running_count();
}

int world_jobs_cpu_running_count(void)
{
    return world_jobs_taskflow_cpu_running_count();
}

void world_jobs_service_pending_uploads(void)
{
    world_jobs_taskflow_service_pending_uploads();
}

void world_jobs_update(const camera_t* camera, bool allow_submissions)
{
    OCT_PROFILE_ZONE("world_jobs_update_taskflow");
    const Uint64 update_start = oct_profile_now_ticks();
    float pending_uploads_ms = 0.0f;
    float executor_ms = 0.0f;
    world_jobs_schedule_report_t schedule_report = {};

    {
        OCT_PROFILE_ZONE("world_jobs.pending_uploads");
        const Uint64 step_start = oct_profile_now_ticks();
        world_jobs_service_pending_uploads();
        pending_uploads_ms = oct_profile_elapsed_ms(step_start);
        world_update_telemetry_record_service_uploads(pending_uploads_ms);
    }
    world_jobs_schedule_update(camera, allow_submissions, &schedule_report);

    const int worker_backlog = worker_backlog_from_report(schedule_report);
    const int worker_pressure = worker_pressure_from_uploads(pending_uploads_ms);
    {
        OCT_PROFILE_ZONE("world_jobs.worker_policy");
        const Uint64 step_start = oct_profile_now_ticks();
        world_jobs_taskflow_update_worker_policy(worker_backlog, worker_pressure);
        executor_ms = oct_profile_elapsed_ms(step_start);
    }

    int running_jobs = world_jobs_taskflow_running_count();
    const runtime_worker_pool_telemetry_t worker_telemetry =
        world_jobs_taskflow_worker_telemetry(worker_backlog, worker_pressure);
    log_world_jobs_spike(oct_profile_elapsed_ms(update_start),
                         pending_uploads_ms,
                         executor_ms,
                         schedule_report.urgent_scan_ms,
                         schedule_report.regular_scan_ms,
                         schedule_report.urgent_probe_max_ms,
                         schedule_report.regular_probe_max_ms,
                         schedule_report.active_chunks,
                         running_jobs,
                         schedule_report.urgent_submissions,
                         schedule_report.regular_submissions,
                         worker_telemetry);
}

void world_jobs_init(SDL_GPUDevice* device)
{
    world_jobs_taskflow_init(device);
}

void world_jobs_free(void)
{
    world_jobs_taskflow_free();
}

int world_jobs_get_worker_count(void)
{
    return world_jobs_taskflow_worker_count();
}

int world_jobs_get_worker_limit(void)
{
    return world_jobs_taskflow_worker_limit();
}

void world_jobs_set_worker_count(int count)
{
    world_jobs_taskflow_set_pending_worker_count(count);
}
