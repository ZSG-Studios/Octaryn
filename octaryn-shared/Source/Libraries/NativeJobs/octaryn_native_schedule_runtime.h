#pragma once

#include "octaryn_native_schedule_policy.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*octaryn_native_schedule_execute_fn)(void* context);

typedef enum octaryn_native_schedule_runtime_job_flags
{
    OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE = 0,
    OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_MAIN_THREAD = 1u << 0,
    OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_BLOCKING = 1u << 1,
    OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_COMMAND_WRITE = 1u << 2,
}
octaryn_native_schedule_runtime_job_flags;

typedef struct octaryn_native_schedule_runtime_job
{
    const char* job_id;
    const octaryn_native_schedule_resource_access* accesses;
    size_t access_count;
    const char* const* runs_after;
    size_t runs_after_count;
    unsigned int flags;
    octaryn_native_schedule_execute_fn execute;
    void* context;
}
octaryn_native_schedule_runtime_job;

typedef struct octaryn_native_schedule_runtime_report
{
    size_t submitted_jobs;
    size_t completed_jobs;
    size_t worker_jobs;
    size_t main_thread_jobs;
    size_t execution_waves;
    int failed_job_index;
}
octaryn_native_schedule_runtime_report;

OCTARYN_NATIVE_JOBS_API void* octaryn_native_schedule_runtime_create(
    int logical_cores,
    int configured_worker_limit);
OCTARYN_NATIVE_JOBS_API void octaryn_native_schedule_runtime_destroy(void* runtime);
OCTARYN_NATIVE_JOBS_API int octaryn_native_schedule_runtime_execute(
    void* runtime,
    const octaryn_native_schedule_runtime_job* jobs,
    size_t job_count,
    octaryn_native_schedule_runtime_report* report);
OCTARYN_NATIVE_JOBS_API void* octaryn_native_schedule_runtime_submit_worker(
    void* runtime,
    const octaryn_native_schedule_runtime_job* jobs,
    size_t job_count);
OCTARYN_NATIVE_JOBS_API int octaryn_native_schedule_runtime_task_ready(
    void* task);
OCTARYN_NATIVE_JOBS_API int octaryn_native_schedule_runtime_task_result(
    void* task,
    octaryn_native_schedule_runtime_report* report);
OCTARYN_NATIVE_JOBS_API void octaryn_native_schedule_runtime_task_destroy(
    void* task);

#ifdef __cplusplus
}
#endif
