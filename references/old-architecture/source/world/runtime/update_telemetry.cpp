#include "world/runtime/update_telemetry.h"

#include <atomic>

namespace {

typedef struct telemetry_bucket
{
    std::atomic<Uint64> count{0};
    std::atomic<Uint64> total_us{0};
    std::atomic<Uint64> max_us{0};
}
telemetry_bucket_t;

telemetry_bucket_t g_update = {};
telemetry_bucket_t g_edit = {};
telemetry_bucket_t g_move = {};
telemetry_bucket_t g_service_uploads = {};
telemetry_bucket_t g_transition = {};
telemetry_bucket_t g_jobs = {};

Uint64 duration_to_us(float duration_ms)
{
    if (duration_ms <= 0.0f)
    {
        return 0;
    }
    return static_cast<Uint64>(duration_ms * 1000.0f + 0.5f);
}

void update_max(std::atomic<Uint64>& max_value, Uint64 value)
{
    Uint64 current = max_value.load(std::memory_order_relaxed);
    while (value > current &&
           !max_value.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed))
    {
    }
}

void record_bucket(telemetry_bucket_t& bucket, float duration_ms)
{
    const Uint64 duration_us = duration_to_us(duration_ms);
    bucket.count.fetch_add(1, std::memory_order_relaxed);
    bucket.total_us.fetch_add(duration_us, std::memory_order_relaxed);
    update_max(bucket.max_us, duration_us);
}

void take_bucket(telemetry_bucket_t& bucket, Uint64* count, Uint64* total_us, Uint64* max_us)
{
    *count = bucket.count.exchange(0, std::memory_order_relaxed);
    *total_us = bucket.total_us.exchange(0, std::memory_order_relaxed);
    *max_us = bucket.max_us.exchange(0, std::memory_order_relaxed);
}

} // namespace

void world_update_telemetry_record_update(float duration_ms)
{
    record_bucket(g_update, duration_ms);
}

void world_update_telemetry_record_edit(float duration_ms)
{
    record_bucket(g_edit, duration_ms);
}

void world_update_telemetry_record_move(float duration_ms)
{
    record_bucket(g_move, duration_ms);
}

void world_update_telemetry_record_service_uploads(float duration_ms)
{
    record_bucket(g_service_uploads, duration_ms);
}

void world_update_telemetry_record_transition(float duration_ms)
{
    record_bucket(g_transition, duration_ms);
}

void world_update_telemetry_record_jobs(float duration_ms)
{
    record_bucket(g_jobs, duration_ms);
}

void world_update_telemetry_take_window(world_update_telemetry_window_t* out_window)
{
    if (!out_window)
    {
        return;
    }

    *out_window = {};
    take_bucket(g_update, &out_window->update_count, &out_window->update_total_us, &out_window->update_max_us);
    take_bucket(g_edit, &out_window->edit_count, &out_window->edit_total_us, &out_window->edit_max_us);
    take_bucket(g_move, &out_window->move_count, &out_window->move_total_us, &out_window->move_max_us);
    take_bucket(g_service_uploads,
                &out_window->service_upload_count,
                &out_window->service_upload_total_us,
                &out_window->service_upload_max_us);
    take_bucket(g_transition,
                &out_window->transition_count,
                &out_window->transition_total_us,
                &out_window->transition_max_us);
    take_bucket(g_jobs, &out_window->jobs_count, &out_window->jobs_total_us, &out_window->jobs_max_us);
}
