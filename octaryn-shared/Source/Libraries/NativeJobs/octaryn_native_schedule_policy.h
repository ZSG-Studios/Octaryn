#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum octaryn_native_schedule_access_mode
{
    OCTARYN_NATIVE_SCHEDULE_ACCESS_READ,
    OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE,
}
octaryn_native_schedule_access_mode;

typedef enum octaryn_native_schedule_job_flags
{
    OCTARYN_NATIVE_SCHEDULE_JOB_NONE = 0,
    OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD = 1u << 0,
    OCTARYN_NATIVE_SCHEDULE_JOB_BLOCKING = 1u << 1,
}
octaryn_native_schedule_job_flags;

typedef struct octaryn_native_schedule_resource_access
{
    const char* resource_id;
    octaryn_native_schedule_access_mode mode;
}
octaryn_native_schedule_resource_access;

typedef struct octaryn_native_schedule_job
{
    const char* job_id;
    const octaryn_native_schedule_resource_access* accesses;
    size_t access_count;
    unsigned int flags;
}
octaryn_native_schedule_job;

typedef struct octaryn_native_schedule_conflict
{
    int has_conflict;
    size_t left_access_index;
    size_t right_access_index;
    const char* resource_id;
}
octaryn_native_schedule_conflict;

int octaryn_native_schedule_accesses_conflict(
    const octaryn_native_schedule_resource_access* left,
    const octaryn_native_schedule_resource_access* right);
int octaryn_native_schedule_jobs_conflict(
    const octaryn_native_schedule_job* left,
    const octaryn_native_schedule_job* right,
    octaryn_native_schedule_conflict* conflict);
int octaryn_native_schedule_job_blocks_main_thread(
    const octaryn_native_schedule_job* job);
int octaryn_native_schedule_jobs_can_run_concurrently(
    const octaryn_native_schedule_job* left,
    const octaryn_native_schedule_job* right,
    octaryn_native_schedule_conflict* conflict);

#ifdef __cplusplus
}
#endif
