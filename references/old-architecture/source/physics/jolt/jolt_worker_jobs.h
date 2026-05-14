#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

class runtime_worker_pool_t;

namespace octaryn::physics {

class JoltRuntimeWorkerJobSystem final : public JPH::JobSystemWithBarrier
{
public:
    explicit JoltRuntimeWorkerJobSystem(runtime_worker_pool_t& worker_pool);
    ~JoltRuntimeWorkerJobSystem() override;

    int GetMaxConcurrency() const override;
    JobHandle CreateJob(const char* name,
                        JPH::ColorArg color,
                        const JobFunction& job_function,
                        JPH::uint32 dependency_count = 0) override;

    int active_job_count() const;
    void wait_for_submitted_jobs();

protected:
    void QueueJob(Job* job) override;
    void QueueJobs(Job** jobs, JPH::uint job_count) override;
    void FreeJob(Job* job) override;

private:
    void submit_job(Job* job);
    void finish_submitted_job();

    runtime_worker_pool_t& worker_pool_;
    std::atomic_int active_jobs_{0};
    std::mutex submitted_jobs_mutex_{};
    std::condition_variable submitted_jobs_done_{};
};

std::unique_ptr<JoltRuntimeWorkerJobSystem> jolt_create_runtime_worker_job_system(runtime_worker_pool_t& worker_pool);

} // namespace octaryn::physics
