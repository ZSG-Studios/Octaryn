#include "octaryn_native_schedule_policy.h"

#include <cstring>

namespace {

bool has_flag(unsigned int flags, octaryn_native_schedule_job_flags flag)
{
    return (flags & static_cast<unsigned int>(flag)) != 0u;
}

bool valid_resource_id(const char* value)
{
    return value != nullptr && value[0] != '\0';
}

void clear_conflict(octaryn_native_schedule_conflict* conflict)
{
    if (conflict == nullptr)
    {
        return;
    }

    *conflict = {};
}

void set_conflict(
    octaryn_native_schedule_conflict* conflict,
    size_t left_index,
    size_t right_index,
    const char* resource_id)
{
    if (conflict == nullptr)
    {
        return;
    }

    conflict->has_conflict = 1;
    conflict->left_access_index = left_index;
    conflict->right_access_index = right_index;
    conflict->resource_id = resource_id;
}

} // namespace

int octaryn_native_schedule_accesses_conflict(
    const octaryn_native_schedule_resource_access* left,
    const octaryn_native_schedule_resource_access* right)
{
    if (left == nullptr || right == nullptr ||
        !valid_resource_id(left->resource_id) ||
        !valid_resource_id(right->resource_id))
    {
        return 0;
    }

    if (std::strcmp(left->resource_id, right->resource_id) != 0)
    {
        return 0;
    }

    return left->mode == OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE ||
        right->mode == OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE;
}

int octaryn_native_schedule_jobs_conflict(
    const octaryn_native_schedule_job* left,
    const octaryn_native_schedule_job* right,
    octaryn_native_schedule_conflict* conflict)
{
    clear_conflict(conflict);
    if (left == nullptr || right == nullptr ||
        left->accesses == nullptr || right->accesses == nullptr)
    {
        return 0;
    }

    for (size_t left_index = 0; left_index < left->access_count; ++left_index)
    {
        for (size_t right_index = 0; right_index < right->access_count; ++right_index)
        {
            const auto* left_access = &left->accesses[left_index];
            const auto* right_access = &right->accesses[right_index];
            if (!octaryn_native_schedule_accesses_conflict(left_access, right_access))
            {
                continue;
            }

            set_conflict(conflict, left_index, right_index, left_access->resource_id);
            return 1;
        }
    }

    return 0;
}

int octaryn_native_schedule_job_blocks_main_thread(
    const octaryn_native_schedule_job* job)
{
    if (job == nullptr)
    {
        return 0;
    }

    return has_flag(job->flags, OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD) &&
        has_flag(job->flags, OCTARYN_NATIVE_SCHEDULE_JOB_BLOCKING);
}

int octaryn_native_schedule_jobs_can_run_concurrently(
    const octaryn_native_schedule_job* left,
    const octaryn_native_schedule_job* right,
    octaryn_native_schedule_conflict* conflict)
{
    if (octaryn_native_schedule_job_blocks_main_thread(left) ||
        octaryn_native_schedule_job_blocks_main_thread(right))
    {
        clear_conflict(conflict);
        return 0;
    }

    return !octaryn_native_schedule_jobs_conflict(left, right, conflict);
}
