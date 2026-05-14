#include "physics/jolt/jolt_worker_jobs.h"

#include <SDL3/SDL.h>

#include "runtime/jobs/runtime_worker_pool.h"

namespace octaryn::physics {

namespace {

constexpr JPH::uint kMaxJoltBarriers = 8;

} // namespace

JoltRuntimeWorkerJobSystem::JoltRuntimeWorkerJobSystem(runtime_worker_pool_t& worker_pool)
    : worker_pool_(worker_pool)
{
    Init(kMaxJoltBarriers);
}

JoltRuntimeWorkerJobSystem::~JoltRuntimeWorkerJobSystem()
{
    wait_for_submitted_jobs();
}

int JoltRuntimeWorkerJobSystem::GetMaxConcurrency() const
{
    return SDL_max(1, worker_pool_.active_workers() + 1);
}

auto JoltRuntimeWorkerJobSystem::CreateJob(const char* name,
                                           JPH::ColorArg color,
                                           const JobFunction& job_function,
                                           JPH::uint32 dependency_count) -> JobHandle
{
    Job* job = new Job(name, color, this, job_function, dependency_count);
    JobHandle handle(job);
    if (dependency_count == 0)
    {
        QueueJob(job);
    }
    return handle;
}

int JoltRuntimeWorkerJobSystem::active_job_count() const
{
    return active_jobs_.load();
}

void JoltRuntimeWorkerJobSystem::QueueJob(Job* job)
{
    submit_job(job);
}

void JoltRuntimeWorkerJobSystem::QueueJobs(Job** jobs, JPH::uint job_count)
{
    for (JPH::uint index = 0; index < job_count; ++index)
    {
        submit_job(jobs[index]);
    }
}

void JoltRuntimeWorkerJobSystem::FreeJob(Job* job)
{
    delete job;
}

void JoltRuntimeWorkerJobSystem::submit_job(Job* job)
{
    job->AddRef();
    active_jobs_.fetch_add(1);

    const bool submitted = worker_pool_.submit([this, job]() {
        job->Execute();
        job->Release();
        finish_submitted_job();
    });
    if (submitted)
    {
        return;
    }

    job->Release();
    finish_submitted_job();
}

void JoltRuntimeWorkerJobSystem::finish_submitted_job()
{
    const int remaining_jobs = active_jobs_.fetch_sub(1) - 1;
    if (remaining_jobs == 0)
    {
        std::lock_guard<std::mutex> lock(submitted_jobs_mutex_);
        submitted_jobs_done_.notify_all();
    }
}

void JoltRuntimeWorkerJobSystem::wait_for_submitted_jobs()
{
    std::unique_lock<std::mutex> lock(submitted_jobs_mutex_);
    submitted_jobs_done_.wait(lock, [this]() {
        return active_jobs_.load() == 0;
    });
}

std::unique_ptr<JoltRuntimeWorkerJobSystem> jolt_create_runtime_worker_job_system(runtime_worker_pool_t& worker_pool)
{
    return std::make_unique<JoltRuntimeWorkerJobSystem>(worker_pool);
}

} // namespace octaryn::physics
