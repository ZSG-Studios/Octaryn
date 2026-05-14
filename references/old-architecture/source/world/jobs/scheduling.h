#pragma once

#include <SDL3/SDL.h>

#include "core/camera/camera.h"

typedef struct world_jobs_schedule_report
{
    float urgent_scan_ms;
    float regular_scan_ms;
    float urgent_probe_max_ms;
    float regular_probe_max_ms;
    Uint32 urgent_submissions;
    Uint32 regular_submissions;
    int active_chunks;
}
world_jobs_schedule_report_t;

void world_jobs_schedule_update(const camera_t* camera, bool allow_submissions, world_jobs_schedule_report_t* report);
