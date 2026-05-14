#include "world/jobs/taskflow/backend.h"
#include "world/jobs/taskflow/internal.h"

#include <atomic>

#include "core/log.h"
#include "core/profile.h"
#include "runtime/jobs/runtime_worker_policy.h"

namespace world_jobs_taskflow_detail {

namespace {

constexpr Uint64 kGrowHysteresisNs = 250000000ull;
constexpr Uint64 kShrinkHysteresisNs = 1000000000ull;
constexpr Uint64 kRebuildIntervalNs = 2000000000ull;

int env_int_clamped(const char* name, int fallback, int minimum, int maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }
    return SDL_clamp(SDL_atoi(value), minimum, maximum);
}

auto env_has_value(const char* name) -> bool
{
    const char* value = SDL_getenv(name);
    return value && value[0] != '\0';
}

auto configured_worker_limit() -> int
{
    return env_has_value("OCTARYN_WORLD_MAX_WORKERS")
        ? env_int_clamped("OCTARYN_WORLD_MAX_WORKERS", OCTARYN_WORLD_JOBS_MAX_WORKERS, 1, OCTARYN_WORLD_JOBS_MAX_WORKERS)
        : OCTARYN_WORLD_JOBS_MAX_WORKERS;
}

auto detect_policy_sample(int current_workers, int backlog, int pressure) -> runtime_worker_policy_sample_t
{
    const int cpu_count = SDL_max(1, SDL_GetNumLogicalCPUCores());
    const int clamped_pressure = SDL_clamp(pressure, 0, 100);
    const int physics_pressure = runtime_worker_pool_shared().active_submissions();
    return runtime_worker_policy_sample(cpu_count,
                                        configured_worker_limit(),
                                        current_workers,
                                        clamped_pressure,
                                        SDL_max(0, backlog),
                                        clamped_pressure,
                                        physics_pressure);
}

auto detect_worker_limit() -> int
{
    return detect_policy_sample(0, 0, 0).max_workers;
}

auto configured_worker_mode() -> runtime_worker_mode_t
{
    if (env_has_value("OCTARYN_WORLD_MANUAL_WORKERS"))
    {
        return RUNTIME_WORKER_MODE_MANUAL;
    }
    return runtime_worker_mode_from_string(SDL_getenv("OCTARYN_WORLD_WORKER_MODE"), RUNTIME_WORKER_MODE_AUTO);
}

auto worker_mode_configured() -> bool
{
    return env_has_value("OCTARYN_WORLD_WORKER_MODE") || env_has_value("OCTARYN_WORLD_MANUAL_WORKERS");
}

auto resolved_worker_mode_ref() -> std::atomic_int&
{
    static std::atomic_int g_worker_mode{RUNTIME_WORKER_MODE_AUTO};
    return g_worker_mode;
}

auto backlog_high_since_ref() -> Uint64&
{
    static Uint64 g_backlog_high_since = 0;
    return g_backlog_high_since;
}

auto pressure_high_since_ref() -> Uint64&
{
    static Uint64 g_pressure_high_since = 0;
    return g_pressure_high_since;
}

auto resolve_auto_worker_count(int current_workers, int backlog, int pressure) -> int
{
    const runtime_worker_policy_sample_t sample = detect_policy_sample(current_workers, backlog, pressure);
    return SDL_clamp(sample.target_workers, sample.min_workers, sample.max_workers);
}

auto resolve_manual_worker_count(int requested_workers) -> int
{
    const int worker_limit = detect_worker_limit();
    const int configured_workers = env_has_value("OCTARYN_WORLD_MANUAL_WORKERS")
        ? env_int_clamped("OCTARYN_WORLD_MANUAL_WORKERS", requested_workers, 1, worker_limit)
        : requested_workers;
    return SDL_clamp(configured_workers, 1, worker_limit);
}

auto resolved_worker_mode() -> runtime_worker_mode_t
{
    const runtime_worker_mode_t mode = worker_mode_configured()
        ? configured_worker_mode()
        : static_cast<runtime_worker_mode_t>(resolved_worker_mode_ref().load());
    resolved_worker_mode_ref().store(mode);
    return mode;
}

auto resolve_worker_count(int requested_workers) -> int
{
    if (resolved_worker_mode() == RUNTIME_WORKER_MODE_MANUAL)
    {
        return resolve_manual_worker_count(requested_workers);
    }
    return resolve_auto_worker_count(active_worker_count(), 1, 0);
}

auto clamp_rebuild_worker_count(int worker_count) -> int
{
    const runtime_worker_policy_sample_t sample = detect_policy_sample(active_worker_count(), 0, 0);
    return SDL_clamp(worker_count, sample.min_workers, sample.max_workers);
}

auto resolve_policy_target(int requested_workers, int backlog, int pressure) -> int
{
    if (resolved_worker_mode() == RUNTIME_WORKER_MODE_MANUAL)
    {
        return resolve_manual_worker_count(requested_workers);
    }

    const int current_workers = active_worker_count();
    if (current_workers <= 0)
    {
        return resolve_auto_worker_count(current_workers, SDL_max(1, backlog), pressure);
    }

    const Uint64 now = oct_profile_now_ticks();
    const bool backlog_high = backlog >= SDL_max(1, current_workers);
    const bool pressure_high = pressure >= 75;

    Uint64& backlog_high_since = backlog_high_since_ref();
    Uint64& pressure_high_since = pressure_high_since_ref();
    backlog_high_since = backlog_high ? (backlog_high_since == 0 ? now : backlog_high_since) : 0;
    pressure_high_since = pressure_high ? (pressure_high_since == 0 ? now : pressure_high_since) : 0;

    if (backlog_high_since != 0 && now - backlog_high_since >= kGrowHysteresisNs)
    {
        return resolve_auto_worker_count(current_workers, backlog, pressure);
    }
    if (pressure_high_since != 0 && now - pressure_high_since >= kShrinkHysteresisNs)
    {
        return resolve_auto_worker_count(current_workers, 0, pressure);
    }
    return current_workers;
}

bool can_rebuild_now()
{
    if (cpu_slot_count() != 0 || ready_upload_count() != 0 || active_world_worker_submissions() != 0)
    {
        return false;
    }

    const Uint64 last_rebuild = last_worker_rebuild_ticks();
    return last_rebuild == 0 || oct_profile_now_ticks() - last_rebuild >= kRebuildIntervalNs;
}

} // namespace

auto clamp_worker_count(int count) -> int
{
    return resolve_worker_count(count);
}

void rebuild_executor(int worker_count)
{
    worker_count = worker_count <= 0 ? resolve_worker_count(worker_count) : clamp_rebuild_worker_count(worker_count);
    start_workers(worker_count);
    const runtime_worker_pool_telemetry_t telemetry = worker_pool_telemetry(0, 0);
    oct_log_infof("Using SDL world jobs backend in %s mode with %d workers (rebuild %.2f ms)",
                  runtime_worker_mode_name(static_cast<runtime_worker_mode_t>(resolved_worker_mode_ref().load())),
                  active_worker_count(),
                  telemetry.rebuild_ms);
}

void update_worker_policy(int backlog, int pressure)
{
    const int target = resolve_policy_target(requested_worker_count(), SDL_max(0, backlog), SDL_clamp(pressure, 0, 100));
    request_worker_count(target);
    if (target != active_worker_count() && can_rebuild_now())
    {
        rebuild_executor(target);
    }
}

} // namespace world_jobs_taskflow_detail

int world_jobs_taskflow_worker_count(void)
{
    return world_jobs_taskflow_detail::requested_worker_count();
}

int world_jobs_taskflow_requested_worker_count(void)
{
    return world_jobs_taskflow_detail::requested_worker_count();
}

int world_jobs_taskflow_active_worker_count(void)
{
    return world_jobs_taskflow_detail::active_worker_count();
}

int world_jobs_taskflow_worker_limit(void)
{
    return world_jobs_taskflow_detail::detect_worker_limit();
}

runtime_worker_pool_telemetry_t world_jobs_taskflow_worker_telemetry(int backlog, int pressure)
{
    return world_jobs_taskflow_detail::worker_pool_telemetry(backlog, pressure);
}

void world_jobs_taskflow_set_pending_worker_count(int count)
{
    if (!world_jobs_taskflow_detail::worker_mode_configured())
    {
        world_jobs_taskflow_detail::resolved_worker_mode_ref().store(count > 0 ? RUNTIME_WORKER_MODE_MANUAL
                                                                               : RUNTIME_WORKER_MODE_AUTO);
    }
    world_jobs_taskflow_detail::request_worker_count(world_jobs_taskflow_detail::clamp_worker_count(count));
}

void world_jobs_taskflow_update_worker_policy(int backlog, int pressure)
{
    world_jobs_taskflow_detail::update_worker_policy(backlog, pressure);
}
