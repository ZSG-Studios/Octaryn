#pragma once

#include "octaryn_client_frame_metrics.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct octaryn_client_frame_profile_sample
{
    float total_ms;
    float accounted_ms;
    float submitted_interval_ms;
    float submitted_accounted_ms;
    float submit_gap_ms;
    float post_submit_tail_ms;
    float app_callback_gap_ms;
    float sim_ms;
    float misc_ms;
    float world_ms;
    float frame_acquire_ms;
    float command_acquire_ms;
    float swapchain_wait_ms;
    float swapchain_acquire_ms;
    float resize_ms;
    float fps_cap_sleep_ms;
    float render_ms;
    float render_setup_ms;
    float render_other_ms;
    float gbuffer_ms;
    float gbuffer_sky_ms;
    float gbuffer_opaque_ms;
    float gbuffer_sprite_ms;
    float post_ms;
    float composite_ms;
    float depth_ms;
    float forward_ms;
    float ui_ms;
    float imgui_ms;
    float swapchain_blit_ms;
    float render_submit_ms;
    float untracked_ms;
} octaryn_client_frame_profile_sample;

typedef struct octaryn_client_frame_profile_snapshot
{
    octaryn_client_frame_profile_sample sample;
    octaryn_client_frame_metrics_snapshot metrics;
} octaryn_client_frame_profile_snapshot;

float octaryn_client_frame_profile_elapsed_ms(uint64_t start_ticks, uint64_t end_ticks);
float octaryn_client_frame_profile_elapsed_ms_since(uint64_t start_ticks);
float octaryn_client_frame_profile_fps_from_ms(float value_ms);
uint32_t octaryn_client_frame_profile_hundredths_from_ms(float value_ms);
uint32_t octaryn_client_frame_profile_hundredths_from_seconds(float value_seconds);
uint32_t octaryn_client_frame_profile_tenths_from_fps(float value_fps);
void octaryn_client_frame_profile_finalize_sample(octaryn_client_frame_profile_sample* sample);

#ifdef __cplusplus
}
#endif
