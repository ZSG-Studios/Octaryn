#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <SDL3/SDL.h>
#include <taskflow/taskflow.hpp>

typedef struct runtime_worker_pool_telemetry
{
    int requested_workers;
    int active_workers;
    float rebuild_ms;
    int backlog;
    int pressure;
}
runtime_worker_pool_telemetry_t;

class runtime_worker_pool_t
{
public:
    using job_t = std::function<void()>;

    auto executor() -> tf::Executor*;
    auto executor() const -> const tf::Executor*;

    int requested_workers() const;
    int active_workers() const;
    int active_submissions() const;
    float last_rebuild_ms() const;
    Uint64 last_rebuild_ticks() const;

    bool submit(job_t job);
    void request_workers(int worker_count);
    void rebuild_now(int worker_count);
    void reset();

private:
    std::unique_ptr<tf::Executor> executor_{};
    std::atomic_int requested_workers_{0};
    std::atomic_int active_workers_{0};
    std::atomic_int active_submissions_{0};
    float last_rebuild_ms_ = 0.0f;
    Uint64 last_rebuild_ticks_ = 0;
};

auto runtime_worker_pool_shared() -> runtime_worker_pool_t&;
