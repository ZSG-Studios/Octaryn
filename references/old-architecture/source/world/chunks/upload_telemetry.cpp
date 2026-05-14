#include "world/chunks/upload_telemetry.h"

#include <atomic>

namespace {

typedef struct upload_telemetry_counters
{
    std::atomic<Uint64> mesh_attempts{0};
    std::atomic<Uint64> mesh_successes{0};
    std::atomic<Uint64> mesh_failures{0};
    std::atomic<Uint64> mesh_stale{0};
    std::atomic<Uint64> mesh_deferred{0};
    std::atomic<Uint64> mesh_bytes{0};
    std::atomic<Uint64> light_attempts{0};
    std::atomic<Uint64> light_successes{0};
    std::atomic<Uint64> light_failures{0};
    std::atomic<Uint64> light_stale{0};
    std::atomic<Uint64> light_deferred{0};
    std::atomic<Uint64> light_bytes{0};
}
upload_telemetry_counters_t;

upload_telemetry_counters_t g_upload_telemetry = {};

auto take_counter(std::atomic<Uint64>& counter) -> Uint64
{
    return counter.exchange(0, std::memory_order_acq_rel);
}

} // namespace

void world_upload_telemetry_record_mesh_result(Uint64 bytes, bool success)
{
    g_upload_telemetry.mesh_attempts.fetch_add(1, std::memory_order_relaxed);
    g_upload_telemetry.mesh_bytes.fetch_add(bytes, std::memory_order_relaxed);
    if (success)
    {
        g_upload_telemetry.mesh_successes.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        g_upload_telemetry.mesh_failures.fetch_add(1, std::memory_order_relaxed);
    }
}

void world_upload_telemetry_record_mesh_stale(void)
{
    g_upload_telemetry.mesh_stale.fetch_add(1, std::memory_order_relaxed);
}

void world_upload_telemetry_record_mesh_deferred(void)
{
    g_upload_telemetry.mesh_deferred.fetch_add(1, std::memory_order_relaxed);
}

void world_upload_telemetry_record_light_result(Uint64 bytes, bool success)
{
    g_upload_telemetry.light_attempts.fetch_add(1, std::memory_order_relaxed);
    g_upload_telemetry.light_bytes.fetch_add(bytes, std::memory_order_relaxed);
    if (success)
    {
        g_upload_telemetry.light_successes.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        g_upload_telemetry.light_failures.fetch_add(1, std::memory_order_relaxed);
    }
}

void world_upload_telemetry_record_light_stale(void)
{
    g_upload_telemetry.light_stale.fetch_add(1, std::memory_order_relaxed);
}

void world_upload_telemetry_record_light_deferred(void)
{
    g_upload_telemetry.light_deferred.fetch_add(1, std::memory_order_relaxed);
}

void world_upload_telemetry_take_window(world_upload_telemetry_window_t* out_stats)
{
    if (!out_stats)
    {
        return;
    }

    *out_stats = {
        .mesh_attempts = take_counter(g_upload_telemetry.mesh_attempts),
        .mesh_successes = take_counter(g_upload_telemetry.mesh_successes),
        .mesh_failures = take_counter(g_upload_telemetry.mesh_failures),
        .mesh_stale = take_counter(g_upload_telemetry.mesh_stale),
        .mesh_deferred = take_counter(g_upload_telemetry.mesh_deferred),
        .mesh_bytes = take_counter(g_upload_telemetry.mesh_bytes),
        .light_attempts = take_counter(g_upload_telemetry.light_attempts),
        .light_successes = take_counter(g_upload_telemetry.light_successes),
        .light_failures = take_counter(g_upload_telemetry.light_failures),
        .light_stale = take_counter(g_upload_telemetry.light_stale),
        .light_deferred = take_counter(g_upload_telemetry.light_deferred),
        .light_bytes = take_counter(g_upload_telemetry.light_bytes),
    };
}
