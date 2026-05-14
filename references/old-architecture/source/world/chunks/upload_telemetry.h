#pragma once

#include <SDL3/SDL.h>

typedef struct world_upload_telemetry_window
{
    Uint64 mesh_attempts;
    Uint64 mesh_successes;
    Uint64 mesh_failures;
    Uint64 mesh_stale;
    Uint64 mesh_deferred;
    Uint64 mesh_bytes;
    Uint64 light_attempts;
    Uint64 light_successes;
    Uint64 light_failures;
    Uint64 light_stale;
    Uint64 light_deferred;
    Uint64 light_bytes;
}
world_upload_telemetry_window_t;

void world_upload_telemetry_record_mesh_result(Uint64 bytes, bool success);
void world_upload_telemetry_record_mesh_stale(void);
void world_upload_telemetry_record_mesh_deferred(void);
void world_upload_telemetry_record_light_result(Uint64 bytes, bool success);
void world_upload_telemetry_record_light_stale(void);
void world_upload_telemetry_record_light_deferred(void);
void world_upload_telemetry_take_window(world_upload_telemetry_window_t* out_stats);
