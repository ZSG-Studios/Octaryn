#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_metrics.h"

typedef struct main_frame_profile_sample
{
    Uint64 frame_start_ticks;
    Uint64 present_submit_ticks;
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
    Uint32 swapchain_skipped;
    Uint32 render_attempted;
    Uint32 swapchain_acquired;
    Uint32 present_submitted;
    float render_ms;
    float render_setup_ms;
    float gbuffer_ms;
    float gbuffer_sky_ms;
    float gbuffer_opaque_ms;
    float gbuffer_sprite_ms;
    float composite_ms;
    float depth_ms;
    float post_ms;
    float forward_ms;
    float ui_ms;
    float imgui_ms;
    float swapchain_blit_ms;
    float render_submit_ms;
    float render_other_ms;
    float untracked_ms;
}
main_frame_profile_sample_t;

typedef enum main_frame_profile_metric
{
    MAIN_FRAME_PROFILE_METRIC_SIM,
    MAIN_FRAME_PROFILE_METRIC_ACCOUNTED,
    MAIN_FRAME_PROFILE_METRIC_SUBMITTED_INTERVAL,
    MAIN_FRAME_PROFILE_METRIC_SUBMITTED_ACCOUNTED,
    MAIN_FRAME_PROFILE_METRIC_SUBMIT_GAP,
    MAIN_FRAME_PROFILE_METRIC_POST_SUBMIT_TAIL,
    MAIN_FRAME_PROFILE_METRIC_APP_CALLBACK_GAP,
    MAIN_FRAME_PROFILE_METRIC_MISC,
    MAIN_FRAME_PROFILE_METRIC_WORLD,
    MAIN_FRAME_PROFILE_METRIC_RENDER,
    MAIN_FRAME_PROFILE_METRIC_RENDER_SETUP,
    MAIN_FRAME_PROFILE_METRIC_RENDER_OTHER,
    MAIN_FRAME_PROFILE_METRIC_GBUFFER,
    MAIN_FRAME_PROFILE_METRIC_GBUFFER_SKY,
    MAIN_FRAME_PROFILE_METRIC_GBUFFER_OPAQUE,
    MAIN_FRAME_PROFILE_METRIC_GBUFFER_SPRITE,
    MAIN_FRAME_PROFILE_METRIC_POST,
    MAIN_FRAME_PROFILE_METRIC_COMPOSITE,
    MAIN_FRAME_PROFILE_METRIC_DEPTH,
    MAIN_FRAME_PROFILE_METRIC_FORWARD,
    MAIN_FRAME_PROFILE_METRIC_UI,
    MAIN_FRAME_PROFILE_METRIC_IMGUI,
    MAIN_FRAME_PROFILE_METRIC_UNTRACKED,
    MAIN_FRAME_PROFILE_METRIC_TOTAL,
    MAIN_FRAME_PROFILE_METRIC_FRAME_ACQUIRE,
    MAIN_FRAME_PROFILE_METRIC_COMMAND_ACQUIRE,
    MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_WAIT,
    MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_ACQUIRE,
    MAIN_FRAME_PROFILE_METRIC_RESIZE,
    MAIN_FRAME_PROFILE_METRIC_FPS_CAP_SLEEP,
    MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_BLIT,
    MAIN_FRAME_PROFILE_METRIC_RENDER_SUBMIT,
    MAIN_FRAME_PROFILE_METRIC_COUNT,
}
main_frame_profile_metric_t;

typedef struct main_frame_profile_snapshot
{
    Uint32 total_hundredths;
    Uint32 accounted_hundredths;
    Uint32 submitted_interval_hundredths;
    Uint32 submitted_accounted_hundredths;
    Uint32 submit_gap_hundredths;
    Uint32 post_submit_tail_hundredths;
    Uint32 app_callback_gap_hundredths;
    Uint32 sim_hundredths;
    Uint32 misc_hundredths;
    Uint32 world_hundredths;
    Uint32 frame_acquire_hundredths;
    Uint32 command_acquire_hundredths;
    Uint32 swapchain_wait_hundredths;
    Uint32 swapchain_acquire_hundredths;
    Uint32 resize_hundredths;
    Uint32 fps_cap_sleep_hundredths;
    Uint32 render_hundredths;
    Uint32 render_setup_hundredths;
    Uint32 render_other_hundredths;
    Uint32 gbuffer_hundredths;
    Uint32 gbuffer_sky_hundredths;
    Uint32 gbuffer_opaque_hundredths;
    Uint32 gbuffer_sprite_hundredths;
    Uint32 post_hundredths;
    Uint32 composite_hundredths;
    Uint32 depth_hundredths;
    Uint32 forward_hundredths;
    Uint32 ui_hundredths;
    Uint32 imgui_hundredths;
    Uint32 swapchain_blit_hundredths;
    Uint32 render_submit_hundredths;
    Uint32 untracked_hundredths;
    Uint32 current_fps_tenths;
    Uint32 current_ms_hundredths;
    Uint32 average_fps_tenths;
    Uint32 average_ms_hundredths;
    Uint32 low_0_1_fps_tenths;
    Uint32 low_0_1_ms_hundredths;
    Uint32 low_1_fps_tenths;
    Uint32 low_1_ms_hundredths;
    Uint32 low_x5_fps_tenths;
    Uint32 low_x5_ms_hundredths;
    Uint32 low_x10_fps_tenths;
    Uint32 low_x10_ms_hundredths;
    Uint32 worst_fps_tenths;
    Uint32 worst_ms_hundredths;
    Uint32 warmup_elapsed_hundredths;
    Uint32 warmup_total_hundredths;
    Uint32 sample_count;
    Uint32 warmup_complete;
    Uint32 low_x5_hits;
    Uint32 low_x10_hits;
}
main_frame_profile_snapshot_t;

typedef struct main_frame_profile
{
    float smoothed_ms[MAIN_FRAME_PROFILE_METRIC_COUNT];
    float latest_ms[MAIN_FRAME_PROFILE_METRIC_COUNT];
    app_frame_metrics_t frame_metrics;
    Uint32 submitted_frame_count;
    Uint32 acquired_frame_count;
    Uint32 skipped_present_count;
}
main_frame_profile_t;

void main_frame_profile_init(main_frame_profile_t* state);
void main_frame_profile_note_frame_start(main_frame_profile_sample_t* sample, Uint64 start_ticks);
void main_frame_profile_note_present_submitted(main_frame_profile_sample_t* sample, Uint64 submit_ticks);
void main_frame_profile_note_frame_end(main_frame_profile_sample_t* sample, Uint64 end_ticks);
void main_frame_profile_finalize_sample(main_frame_profile_sample_t* sample);
void main_frame_profile_record(main_frame_profile_t* state, const main_frame_profile_sample_t* sample);
main_frame_profile_snapshot_t main_frame_profile_snapshot(const main_frame_profile_t* state);
app_frame_metrics_snapshot_t main_frame_profile_metrics_snapshot(const main_frame_profile_t* state);
