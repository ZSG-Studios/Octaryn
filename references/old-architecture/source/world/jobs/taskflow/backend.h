#pragma once

#include <SDL3/SDL.h>

#include "runtime/jobs/runtime_worker_pool.h"

bool world_jobs_taskflow_try_update_blocks(int x, int z);
bool world_jobs_taskflow_try_update_meshes_or_lights(int x, int z);
bool world_jobs_taskflow_try_update_urgent_chunk(int x, int z);
bool world_jobs_taskflow_regular_slot_available(void);
int world_jobs_taskflow_running_count(void);
int world_jobs_taskflow_cpu_running_count(void);
int world_jobs_taskflow_upload_waiting_count(void);
int world_jobs_taskflow_worker_count(void);
int world_jobs_taskflow_requested_worker_count(void);
int world_jobs_taskflow_active_worker_count(void);
int world_jobs_taskflow_worker_limit(void);
runtime_worker_pool_telemetry_t world_jobs_taskflow_worker_telemetry(int backlog, int pressure);
void world_jobs_taskflow_service_pending_uploads(void);
void world_jobs_taskflow_init(SDL_GPUDevice* device);
void world_jobs_taskflow_free(void);
void world_jobs_taskflow_set_pending_worker_count(int count);
void world_jobs_taskflow_update_worker_policy(int backlog, int pressure);
