#include "octaryn_native_schedule_runtime.h"

#include "octaryn_native_worker_policy.h"

#include <algorithm>
#include <bit>
#include <memory>
#include <new>
#include <string_view>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace {

struct schedule_runtime
{
    explicit schedule_runtime(int workers)
        : executor(static_cast<unsigned>(std::max(1, workers)))
    {
    }

    tf::Executor executor;
};

bool valid_job(const octaryn_native_schedule_runtime_job& job)
{
    const octaryn_native_schedule_job policy_job = {
        job.job_id,
        job.accesses,
        job.access_count,
        job.flags &
            (OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD |
             OCTARYN_NATIVE_SCHEDULE_JOB_BLOCKING)};
    return job.job_id != nullptr && job.job_id[0] != '\0' &&
        !octaryn_native_schedule_job_blocks_main_thread(&policy_job);
}

bool has_flag(const octaryn_native_schedule_runtime_job& job, unsigned int flag)
{
    return (job.flags & flag) != 0u;
}

bool is_main_thread_job(const octaryn_native_schedule_runtime_job& job)
{
    return has_flag(job, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_MAIN_THREAD);
}

bool dependency_completed(
    const octaryn_native_schedule_runtime_job& job,
    const std::vector<unsigned char>& completed,
    const octaryn_native_schedule_runtime_job* jobs,
    size_t job_count)
{
    for (size_t dependency_index = 0; dependency_index < job.runs_after_count; ++dependency_index)
    {
        if (job.runs_after == nullptr)
        {
            return false;
        }

        const char* dependency = job.runs_after[dependency_index];
        if (dependency == nullptr || dependency[0] == '\0')
        {
            return false;
        }

        bool found_completed = false;
        for (size_t job_index = 0; job_index < job_count; ++job_index)
        {
            if (std::string_view(jobs[job_index].job_id) != dependency)
            {
                continue;
            }

            found_completed = completed[job_index] != 0;
            break;
        }

        if (!found_completed)
        {
            return false;
        }
    }

    return true;
}

bool conflicts_with_batch(
    const octaryn_native_schedule_runtime_job& candidate,
    const std::vector<size_t>& batch,
    const octaryn_native_schedule_runtime_job* jobs)
{
    octaryn_native_schedule_conflict conflict = {};
    const octaryn_native_schedule_job candidate_policy = {
        candidate.job_id,
        candidate.accesses,
        candidate.access_count,
        candidate.flags};
    for (size_t batch_job_index : batch)
    {
        const auto& batch_job = jobs[batch_job_index];
        const octaryn_native_schedule_job batch_policy = {
            batch_job.job_id,
            batch_job.accesses,
            batch_job.access_count,
            batch_job.flags};
        if (octaryn_native_schedule_jobs_conflict(&candidate_policy, &batch_policy, &conflict) != 0)
        {
            return true;
        }
    }

    return false;
}

int execute_job(const octaryn_native_schedule_runtime_job& job)
{
    if (job.execute == nullptr)
    {
        return 0;
    }

    const bool command_write =
        has_flag(job, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_COMMAND_WRITE);
    if (command_write)
    {
        octaryn_native_command_write_scope_enter();
    }

    const int result = job.execute(job.context);
    if (command_write)
    {
        octaryn_native_command_write_scope_exit();
    }

    return result;
}

void clear_report(octaryn_native_schedule_runtime_report* report)
{
    if (report == nullptr)
    {
        return;
    }

    *report = {};
    report->failed_job_index = -1;
}

} // namespace

void* octaryn_native_schedule_runtime_create(
    int logical_cores,
    int configured_worker_limit)
{
    const int workers =
        octaryn_native_worker_policy_maximum_workers(logical_cores, configured_worker_limit);
    return new (std::nothrow) schedule_runtime(workers);
}

void octaryn_native_schedule_runtime_destroy(void* runtime)
{
    delete static_cast<schedule_runtime*>(runtime);
}

int octaryn_native_schedule_runtime_execute(
    void* runtime,
    const octaryn_native_schedule_runtime_job* jobs,
    size_t job_count,
    octaryn_native_schedule_runtime_report* report)
{
    clear_report(report);
    auto* scheduler = static_cast<schedule_runtime*>(runtime);
    if (scheduler == nullptr || (job_count > 0 && jobs == nullptr))
    {
        return -1;
    }

    std::vector<unsigned char> completed(job_count, 0);
    size_t completed_count = 0;
    if (report != nullptr)
    {
        report->submitted_jobs = job_count;
    }

    while (completed_count < job_count)
    {
        std::vector<size_t> main_thread_batch;
        std::vector<size_t> worker_batch;
        for (size_t job_index = 0; job_index < job_count; ++job_index)
        {
            const auto& job = jobs[job_index];
            if (completed[job_index] != 0 || !valid_job(job) ||
                !dependency_completed(job, completed, jobs, job_count))
            {
                continue;
            }

            auto& batch = is_main_thread_job(job) ? main_thread_batch : worker_batch;
            if (!conflicts_with_batch(job, batch, jobs))
            {
                batch.push_back(job_index);
            }
        }

        if (main_thread_batch.empty() && worker_batch.empty())
        {
            return -2;
        }

        if (report != nullptr)
        {
            report->execution_waves++;
            report->main_thread_jobs += main_thread_batch.size();
            report->worker_jobs += worker_batch.size();
        }

        for (size_t job_index : main_thread_batch)
        {
            const int result = execute_job(jobs[job_index]);
            if (result != 0)
            {
                if (report != nullptr)
                {
                    report->failed_job_index = static_cast<int>(job_index);
                }
                return result;
            }

            completed[job_index] = 1;
            completed_count++;
        }

        tf::Taskflow taskflow;
        std::vector<int> worker_results(worker_batch.size(), 0);
        for (size_t index = 0; index < worker_batch.size(); ++index)
        {
            const size_t job_index = worker_batch[index];
            taskflow.emplace([jobs, job_index, &worker_results, index] {
                worker_results[index] = execute_job(jobs[job_index]);
            });
        }
        scheduler->executor.run(taskflow).wait();

        for (size_t index = 0; index < worker_batch.size(); ++index)
        {
            const size_t job_index = worker_batch[index];
            if (worker_results[index] != 0)
            {
                if (report != nullptr)
                {
                    report->failed_job_index = static_cast<int>(job_index);
                }
                return worker_results[index];
            }

            completed[job_index] = 1;
            completed_count++;
        }
    }

    if (report != nullptr)
    {
        report->completed_jobs = completed_count;
    }
    return 0;
}
