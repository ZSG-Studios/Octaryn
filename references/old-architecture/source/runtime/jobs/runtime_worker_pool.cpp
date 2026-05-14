#include "runtime/jobs/runtime_worker_pool.h"

#include "core/profile.h"

#include <utility>

auto runtime_worker_pool_t::executor() -> tf::Executor*
{
    return executor_.get();
}

auto runtime_worker_pool_t::executor() const -> const tf::Executor*
{
    return executor_.get();
}

int runtime_worker_pool_t::requested_workers() const
{
    return requested_workers_.load();
}

int runtime_worker_pool_t::active_workers() const
{
    return active_workers_.load();
}

int runtime_worker_pool_t::active_submissions() const
{
    return active_submissions_.load();
}

float runtime_worker_pool_t::last_rebuild_ms() const
{
    return last_rebuild_ms_;
}

Uint64 runtime_worker_pool_t::last_rebuild_ticks() const
{
    return last_rebuild_ticks_;
}

void runtime_worker_pool_t::request_workers(int worker_count)
{
    requested_workers_.store(worker_count);
}

bool runtime_worker_pool_t::submit(job_t job)
{
    if (!job || executor_ == nullptr)
    {
        return false;
    }

    active_submissions_.fetch_add(1);
    executor_->silent_async([this, job = std::move(job)]() mutable {
        job();
        active_submissions_.fetch_sub(1);
    });
    return true;
}

void runtime_worker_pool_t::rebuild_now(int worker_count)
{
    const Uint64 rebuild_start = oct_profile_now_ticks();
    executor_ = std::make_unique<tf::Executor>(worker_count);
    active_workers_.store(worker_count);
    requested_workers_.store(worker_count);
    last_rebuild_ms_ = oct_profile_elapsed_ms(rebuild_start);
    last_rebuild_ticks_ = oct_profile_now_ticks();
}

void runtime_worker_pool_t::reset()
{
    executor_.reset();
    requested_workers_.store(0);
    active_workers_.store(0);
    active_submissions_.store(0);
    last_rebuild_ms_ = 0.0f;
    last_rebuild_ticks_ = 0;
}

auto runtime_worker_pool_shared() -> runtime_worker_pool_t&
{
    static runtime_worker_pool_t g_worker_pool{};
    return g_worker_pool;
}
