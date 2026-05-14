#include "runtime/jobs/runtime_worker_policy.h"

#include <SDL3/SDL.h>

namespace {

auto normalize_core_count(int logical_cores) -> int
{
    return SDL_max(1, logical_cores);
}

auto default_max_workers(int logical_cores) -> int
{
    logical_cores = normalize_core_count(logical_cores);
    if (logical_cores <= 2)
    {
        return 1;
    }
    if (logical_cores <= 7)
    {
        return logical_cores - 1;
    }
    if (logical_cores <= 15)
    {
        return logical_cores - 2;
    }
    return logical_cores - 3;
}

} // namespace

int runtime_worker_policy_min_workers(int logical_cores)
{
    logical_cores = normalize_core_count(logical_cores);
    if (logical_cores <= 7)
    {
        return 1;
    }
    if (logical_cores <= 15)
    {
        return 2;
    }
    return 3;
}

int runtime_worker_policy_max_workers(int logical_cores, int configured_worker_limit)
{
    const int policy_limit = default_max_workers(logical_cores);
    if (configured_worker_limit <= 0)
    {
        return policy_limit;
    }
    return SDL_clamp(configured_worker_limit, 1, policy_limit);
}

int runtime_worker_policy_target_workers(const runtime_worker_policy_sample_t* sample)
{
    if (sample == nullptr)
    {
        return 1;
    }

    const int min_workers = SDL_max(1, SDL_min(sample->min_workers, sample->max_workers));
    const int max_workers = SDL_max(min_workers, sample->max_workers);
    const int current_workers = SDL_clamp(sample->current_workers, min_workers, max_workers);
    const int pressure = SDL_clamp(sample->render_pressure + sample->upload_pressure, 0, 100);
    const int pressure_reserve = pressure >= 75 ? 2 : pressure >= 50 ? 1 : 0;
    const int pressure_limited_max = SDL_max(min_workers, max_workers - pressure_reserve);
    const int demand = sample->chunk_backlog + sample->physics_pressure;

    if (sample->current_workers <= 0 || demand > 0)
    {
        return pressure_limited_max;
    }
    return SDL_clamp(current_workers, min_workers, pressure_limited_max);
}

runtime_worker_policy_sample_t runtime_worker_policy_sample(int logical_cores,
                                                            int configured_worker_limit,
                                                            int current_workers,
                                                            int render_pressure,
                                                            int chunk_backlog,
                                                            int upload_pressure,
                                                            int physics_pressure)
{
    runtime_worker_policy_sample_t sample = {};
    sample.logical_cores = normalize_core_count(logical_cores);
    sample.render_pressure = SDL_max(0, render_pressure);
    sample.chunk_backlog = SDL_max(0, chunk_backlog);
    sample.upload_pressure = SDL_max(0, upload_pressure);
    sample.physics_pressure = SDL_max(0, physics_pressure);
    sample.current_workers = SDL_max(0, current_workers);
    sample.min_workers = runtime_worker_policy_min_workers(sample.logical_cores);
    sample.max_workers = runtime_worker_policy_max_workers(sample.logical_cores, configured_worker_limit);
    sample.min_workers = SDL_min(sample.min_workers, sample.max_workers);
    sample.target_workers = runtime_worker_policy_target_workers(&sample);
    return sample;
}

runtime_worker_mode_t runtime_worker_mode_from_string(const char* value, runtime_worker_mode_t fallback)
{
    if (value == nullptr || value[0] == '\0')
    {
        return fallback;
    }
    if (SDL_strcasecmp(value, "auto") == 0)
    {
        return RUNTIME_WORKER_MODE_AUTO;
    }
    if (SDL_strcasecmp(value, "manual") == 0)
    {
        return RUNTIME_WORKER_MODE_MANUAL;
    }
    return fallback;
}

const char* runtime_worker_mode_name(runtime_worker_mode_t mode)
{
    return mode == RUNTIME_WORKER_MODE_MANUAL ? "Manual" : "Auto";
}
