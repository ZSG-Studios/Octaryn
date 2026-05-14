#pragma once

typedef enum runtime_worker_mode
{
    RUNTIME_WORKER_MODE_AUTO,
    RUNTIME_WORKER_MODE_MANUAL,
}
runtime_worker_mode_t;

typedef struct runtime_worker_policy_sample
{
    int logical_cores;
    int render_pressure;
    int chunk_backlog;
    int upload_pressure;
    int physics_pressure;
    int current_workers;
    int min_workers;
    int max_workers;
    int target_workers;
}
runtime_worker_policy_sample_t;

runtime_worker_policy_sample_t runtime_worker_policy_sample(int logical_cores,
                                                             int configured_worker_limit,
                                                             int current_workers,
                                                             int render_pressure,
                                                             int chunk_backlog,
                                                             int upload_pressure,
                                                             int physics_pressure);
int runtime_worker_policy_min_workers(int logical_cores);
int runtime_worker_policy_max_workers(int logical_cores, int configured_worker_limit);
int runtime_worker_policy_target_workers(const runtime_worker_policy_sample_t* sample);
runtime_worker_mode_t runtime_worker_mode_from_string(const char* value, runtime_worker_mode_t fallback);
const char* runtime_worker_mode_name(runtime_worker_mode_t mode);
