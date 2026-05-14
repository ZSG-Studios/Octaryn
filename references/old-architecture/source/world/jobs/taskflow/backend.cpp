#include "world/jobs/taskflow/backend.h"

#include "world/jobs/taskflow/internal.h"

int world_jobs_taskflow_running_count(void)
{
    return world_jobs_taskflow_detail::busy_slot_count().load();
}

int world_jobs_taskflow_cpu_running_count(void)
{
    return world_jobs_taskflow_detail::cpu_slot_count();
}

int world_jobs_taskflow_upload_waiting_count(void)
{
    return world_jobs_taskflow_detail::ready_upload_count();
}

void world_jobs_taskflow_service_pending_uploads(void)
{
    world_jobs_taskflow_detail::commit_ready_uploads(false);
}

void world_jobs_taskflow_init(SDL_GPUDevice* device)
{
    world_jobs_taskflow_detail::init_slots(device);
    world_jobs_taskflow_detail::rebuild_executor(world_jobs_taskflow_detail::requested_worker_count());
}

void world_jobs_taskflow_free(void)
{
    world_jobs_taskflow_detail::stop_workers();
    world_jobs_taskflow_detail::commit_ready_uploads(true);
    world_jobs_taskflow_detail::free_slots();
}
