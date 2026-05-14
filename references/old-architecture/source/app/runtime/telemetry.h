#pragma once

#include <SDL3/SDL.h>

typedef struct main_runtime_telemetry_snapshot
{
    Uint32 cpu_ram_hundredths_gib;
    Uint32 gpu_vram_hundredths_gib;
    Uint32 cpu_load_hundredths;
    Uint32 gpu_load_hundredths;
}
main_runtime_telemetry_snapshot_t;

void app_runtime_telemetry_init(void);
main_runtime_telemetry_snapshot_t app_runtime_telemetry_sample(void);
