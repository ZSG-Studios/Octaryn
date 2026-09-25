#include "octaryn_native_worker_policy.h"

#include <algorithm>
#include <cctype>

namespace {

constexpr int MinimumWorkerThreads = 2;

int normalize_core_count(int logical_cores)
{
    return std::max(1, logical_cores);
}

int default_maximum_workers(int logical_cores)
{
    logical_cores = normalize_core_count(logical_cores);
    if (logical_cores <= MinimumWorkerThreads)
    {
        return MinimumWorkerThreads;
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

bool equals_ignore_case(const char* left, const char* right)
{
    if (left == nullptr || right == nullptr)
    {
        return false;
    }

    while (*left != '\0' && *right != '\0')
    {
        const auto left_char = static_cast<unsigned char>(*left);
        const auto right_char = static_cast<unsigned char>(*right);
        if (std::tolower(left_char) != std::tolower(right_char))
        {
            return false;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

} // namespace

int octaryn_native_worker_policy_minimum_workers(int logical_cores)
{
    (void)logical_cores;
    return MinimumWorkerThreads;
}

int octaryn_native_worker_policy_maximum_workers(int logical_cores, int configured_worker_limit)
{
    const int policy_limit = std::max(MinimumWorkerThreads, default_maximum_workers(logical_cores));
    if (configured_worker_limit <= 0)
    {
        return policy_limit;
    }
    return std::clamp(configured_worker_limit, MinimumWorkerThreads, policy_limit);
}

int octaryn_native_worker_policy_target_workers(const octaryn_native_worker_policy_sample* sample)
{
    if (sample == nullptr)
    {
        return MinimumWorkerThreads;
    }

    const int minimum_workers = std::max(MinimumWorkerThreads, std::min(sample->minimum_workers, sample->maximum_workers));
    const int maximum_workers = std::max(minimum_workers, sample->maximum_workers);
    const int current_workers = std::clamp(sample->current_workers, minimum_workers, maximum_workers);
    const int pressure = std::clamp(sample->render_pressure + sample->upload_pressure, 0, 100);
    const int pressure_reserve = pressure >= 75 ? 2 : pressure >= 50 ? 1 : 0;
    const int pressure_limited_maximum = std::max(minimum_workers, maximum_workers - pressure_reserve);
    const int demand = sample->job_backlog + sample->simulation_pressure;

    if (sample->current_workers <= 0 || demand > 0)
    {
        return pressure_limited_maximum;
    }
    return std::clamp(current_workers, minimum_workers, pressure_limited_maximum);
}

octaryn_native_worker_policy_sample octaryn_native_worker_policy_sample_create(
    int logical_cores,
    int configured_worker_limit,
    int current_workers,
    int render_pressure,
    int job_backlog,
    int upload_pressure,
    int simulation_pressure)
{
    octaryn_native_worker_policy_sample sample = {};
    sample.logical_cores = normalize_core_count(logical_cores);
    sample.render_pressure = std::max(0, render_pressure);
    sample.job_backlog = std::max(0, job_backlog);
    sample.upload_pressure = std::max(0, upload_pressure);
    sample.simulation_pressure = std::max(0, simulation_pressure);
    sample.current_workers = std::max(0, current_workers);
    sample.minimum_workers = octaryn_native_worker_policy_minimum_workers(sample.logical_cores);
    sample.maximum_workers = octaryn_native_worker_policy_maximum_workers(sample.logical_cores, configured_worker_limit);
    sample.minimum_workers = std::min(sample.minimum_workers, sample.maximum_workers);
    sample.target_workers = octaryn_native_worker_policy_target_workers(&sample);
    return sample;
}

octaryn_native_worker_mode octaryn_native_worker_mode_from_string(const char* value, octaryn_native_worker_mode fallback)
{
    if (value == nullptr || value[0] == '\0')
    {
        return fallback;
    }
    if (equals_ignore_case(value, "auto"))
    {
        return OCTARYN_NATIVE_WORKER_MODE_AUTO;
    }
    if (equals_ignore_case(value, "manual"))
    {
        return OCTARYN_NATIVE_WORKER_MODE_MANUAL;
    }
    return fallback;
}

const char* octaryn_native_worker_mode_name(octaryn_native_worker_mode mode)
{
    return mode == OCTARYN_NATIVE_WORKER_MODE_MANUAL ? "Manual" : "Auto";
}
