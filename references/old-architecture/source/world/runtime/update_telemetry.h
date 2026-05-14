#pragma once

#include <SDL3/SDL.h>

typedef struct world_update_telemetry_window
{
    Uint64 update_count;
    Uint64 edit_count;
    Uint64 move_count;
    Uint64 service_upload_count;
    Uint64 transition_count;
    Uint64 jobs_count;
    Uint64 update_total_us;
    Uint64 edit_total_us;
    Uint64 move_total_us;
    Uint64 service_upload_total_us;
    Uint64 transition_total_us;
    Uint64 jobs_total_us;
    Uint64 update_max_us;
    Uint64 edit_max_us;
    Uint64 move_max_us;
    Uint64 service_upload_max_us;
    Uint64 transition_max_us;
    Uint64 jobs_max_us;
}
world_update_telemetry_window_t;

void world_update_telemetry_record_update(float duration_ms);
void world_update_telemetry_record_edit(float duration_ms);
void world_update_telemetry_record_move(float duration_ms);
void world_update_telemetry_record_service_uploads(float duration_ms);
void world_update_telemetry_record_transition(float duration_ms);
void world_update_telemetry_record_jobs(float duration_ms);
void world_update_telemetry_take_window(world_update_telemetry_window_t* out_window);
