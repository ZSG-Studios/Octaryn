#include "frame_profile.h"

namespace {

constexpr float kSmoothingAlpha = 0.15f;
Uint64 g_last_present_submit_ticks = 0;
float g_last_submit_interval_tail_ms = 0.0f;

auto smooth_value(float current, float sample) -> float
{
    if (current <= 0.0f)
    {
        return sample;
    }
    return current + (sample - current) * kSmoothingAlpha;
}

auto to_hundredths(float value_ms) -> Uint32
{
    const float clamped = value_ms < 0.0f ? 0.0f : value_ms;
    return static_cast<Uint32>(clamped * 100.0f + 0.5f);
}

auto to_tenths(float value) -> Uint32
{
    const float clamped = value < 0.0f ? 0.0f : value;
    return static_cast<Uint32>(clamped * 10.0f + 0.5f);
}

} // namespace

void main_frame_profile_init(main_frame_profile_t* state)
{
    if (!state)
    {
        return;
    }
    *state = {};
    app_frame_metrics_init(&state->frame_metrics);
    g_last_present_submit_ticks = 0;
    g_last_submit_interval_tail_ms = 0.0f;
}

void main_frame_profile_note_frame_start(main_frame_profile_sample_t* sample, Uint64 start_ticks)
{
    if (!sample)
    {
        return;
    }
    sample->frame_start_ticks = start_ticks;
}

void main_frame_profile_note_present_submitted(main_frame_profile_sample_t* sample, Uint64 submit_ticks)
{
    if (!sample)
    {
        return;
    }

    sample->present_submit_ticks = submit_ticks;
    if (g_last_present_submit_ticks != 0u && submit_ticks > g_last_present_submit_ticks)
    {
        sample->submitted_interval_ms = static_cast<float>(submit_ticks - g_last_present_submit_ticks) * 1e-6f;
    }

    float frame_start_to_submit_ms = 0.0f;
    if (sample->frame_start_ticks != 0u && submit_ticks > sample->frame_start_ticks)
    {
        frame_start_to_submit_ms = static_cast<float>(submit_ticks - sample->frame_start_ticks) * 1e-6f;
    }
    sample->submitted_accounted_ms =
        g_last_submit_interval_tail_ms + sample->app_callback_gap_ms + frame_start_to_submit_ms;
    sample->submit_gap_ms = SDL_max(0.0f, sample->submitted_interval_ms - sample->submitted_accounted_ms);
    g_last_present_submit_ticks = submit_ticks;
}

void main_frame_profile_note_frame_end(main_frame_profile_sample_t* sample, Uint64 end_ticks)
{
    if (!sample || sample->present_submit_ticks == 0u || end_ticks <= sample->present_submit_ticks)
    {
        return;
    }

    const float raw_tail_ms = static_cast<float>(end_ticks - sample->present_submit_ticks) * 1e-6f;
    const float paced_sleep_ms = SDL_max(0.0f, sample->fps_cap_sleep_ms);
    sample->post_submit_tail_ms = SDL_max(0.0f, raw_tail_ms - paced_sleep_ms);
    g_last_submit_interval_tail_ms = sample->post_submit_tail_ms + paced_sleep_ms;
}

void main_frame_profile_finalize_sample(main_frame_profile_sample_t* sample)
{
    if (!sample)
    {
        return;
    }

    sample->post_ms = sample->composite_ms + sample->depth_ms;

    const float render_breakdown_ms = sample->render_setup_ms +
        sample->gbuffer_ms +
        sample->post_ms +
        sample->forward_ms +
        sample->ui_ms +
        sample->imgui_ms +
        sample->swapchain_blit_ms +
        sample->render_submit_ms;
    sample->render_other_ms = SDL_max(0.0f, sample->render_ms - render_breakdown_ms);

    sample->accounted_ms = sample->sim_ms +
        sample->misc_ms +
        sample->world_ms +
        sample->frame_acquire_ms +
        sample->command_acquire_ms +
        sample->fps_cap_sleep_ms +
        sample->render_ms;
    sample->untracked_ms = SDL_max(0.0f, sample->total_ms - sample->accounted_ms);
}

void main_frame_profile_record(main_frame_profile_t* state, const main_frame_profile_sample_t* sample)
{
    if (!state || !sample)
    {
        return;
    }

    main_frame_profile_sample_t finalized = *sample;
    main_frame_profile_finalize_sample(&finalized);

    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SIM] = finalized.sim_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_ACCOUNTED] = finalized.accounted_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_INTERVAL] = finalized.submitted_interval_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_ACCOUNTED] = finalized.submitted_accounted_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMIT_GAP] = finalized.submit_gap_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_POST_SUBMIT_TAIL] = finalized.post_submit_tail_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_APP_CALLBACK_GAP] = finalized.app_callback_gap_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_MISC] = finalized.misc_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_WORLD] = finalized.world_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER] = finalized.render_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SETUP] = finalized.render_setup_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_OTHER] = finalized.render_other_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER] = finalized.gbuffer_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SKY] = finalized.gbuffer_sky_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_OPAQUE] = finalized.gbuffer_opaque_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SPRITE] = finalized.gbuffer_sprite_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_POST] = finalized.post_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_COMPOSITE] = finalized.composite_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_DEPTH] = finalized.depth_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FORWARD] = finalized.forward_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_UI] = finalized.ui_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_IMGUI] = finalized.imgui_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_UNTRACKED] = finalized.untracked_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_TOTAL] = finalized.total_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FRAME_ACQUIRE] = finalized.frame_acquire_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_COMMAND_ACQUIRE] = finalized.command_acquire_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_WAIT] = finalized.swapchain_wait_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_ACQUIRE] = finalized.swapchain_acquire_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RESIZE] = finalized.resize_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FPS_CAP_SLEEP] = finalized.fps_cap_sleep_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_BLIT] = finalized.swapchain_blit_ms;
    state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SUBMIT] = finalized.render_submit_ms;
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SIM] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SIM], finalized.sim_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_ACCOUNTED] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_ACCOUNTED], finalized.accounted_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_INTERVAL] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_INTERVAL], finalized.submitted_interval_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_ACCOUNTED] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_ACCOUNTED], finalized.submitted_accounted_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMIT_GAP] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SUBMIT_GAP], finalized.submit_gap_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_POST_SUBMIT_TAIL] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_POST_SUBMIT_TAIL], finalized.post_submit_tail_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_APP_CALLBACK_GAP] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_APP_CALLBACK_GAP], finalized.app_callback_gap_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_MISC] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_MISC], finalized.misc_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_WORLD] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_WORLD], finalized.world_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER], finalized.render_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SETUP] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SETUP], finalized.render_setup_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_OTHER] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_OTHER], finalized.render_other_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER], finalized.gbuffer_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SKY] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SKY], finalized.gbuffer_sky_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_OPAQUE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_OPAQUE], finalized.gbuffer_opaque_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SPRITE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SPRITE], finalized.gbuffer_sprite_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_POST] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_POST], finalized.post_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_COMPOSITE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_COMPOSITE], finalized.composite_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_DEPTH] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_DEPTH], finalized.depth_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FORWARD] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FORWARD], finalized.forward_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_UI] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_UI], finalized.ui_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_IMGUI] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_IMGUI], finalized.imgui_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_UNTRACKED] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_UNTRACKED], finalized.untracked_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_TOTAL] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_TOTAL], finalized.total_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FRAME_ACQUIRE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FRAME_ACQUIRE], finalized.frame_acquire_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_COMMAND_ACQUIRE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_COMMAND_ACQUIRE], finalized.command_acquire_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_WAIT] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_WAIT], finalized.swapchain_wait_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_ACQUIRE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_ACQUIRE], finalized.swapchain_acquire_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RESIZE] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RESIZE], finalized.resize_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FPS_CAP_SLEEP] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_FPS_CAP_SLEEP], finalized.fps_cap_sleep_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_BLIT] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_BLIT], finalized.swapchain_blit_ms);
    state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SUBMIT] =
        smooth_value(state->smoothed_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SUBMIT], finalized.render_submit_ms);
    if (finalized.present_submitted && finalized.submitted_interval_ms > 0.0f)
    {
        app_frame_metrics_record(&state->frame_metrics, finalized.submitted_interval_ms, finalized.present_submit_ticks);
        ++state->submitted_frame_count;
    }
    if (finalized.swapchain_acquired)
    {
        ++state->acquired_frame_count;
    }
    if (finalized.swapchain_skipped)
    {
        ++state->skipped_present_count;
    }
}

main_frame_profile_snapshot_t main_frame_profile_snapshot(const main_frame_profile_t* state)
{
    if (!state)
    {
        return {};
    }

    const app_frame_metrics_snapshot_t metrics =
        app_frame_metrics_snapshot(&state->frame_metrics, SDL_GetTicksNS());

    return {
        .total_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_TOTAL]),
        .accounted_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_ACCOUNTED]),
        .submitted_interval_hundredths =
            to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_INTERVAL]),
        .submitted_accounted_hundredths =
            to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMITTED_ACCOUNTED]),
        .submit_gap_hundredths =
            to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SUBMIT_GAP]),
        .post_submit_tail_hundredths =
            to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_POST_SUBMIT_TAIL]),
        .app_callback_gap_hundredths =
            to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_APP_CALLBACK_GAP]),
        .sim_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SIM]),
        .misc_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_MISC]),
        .world_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_WORLD]),
        .frame_acquire_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FRAME_ACQUIRE]),
        .command_acquire_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_COMMAND_ACQUIRE]),
        .swapchain_wait_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_WAIT]),
        .swapchain_acquire_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_ACQUIRE]),
        .resize_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RESIZE]),
        .fps_cap_sleep_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FPS_CAP_SLEEP]),
        .render_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER]),
        .render_setup_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SETUP]),
        .render_other_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_OTHER]),
        .gbuffer_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER]),
        .gbuffer_sky_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SKY]),
        .gbuffer_opaque_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_OPAQUE]),
        .gbuffer_sprite_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_GBUFFER_SPRITE]),
        .post_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_POST]),
        .composite_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_COMPOSITE]),
        .depth_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_DEPTH]),
        .forward_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_FORWARD]),
        .ui_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_UI]),
        .imgui_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_IMGUI]),
        .swapchain_blit_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_SWAPCHAIN_BLIT]),
        .render_submit_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_RENDER_SUBMIT]),
        .untracked_hundredths = to_hundredths(state->latest_ms[MAIN_FRAME_PROFILE_METRIC_UNTRACKED]),
        .current_fps_tenths = to_tenths(metrics.current.fps),
        .current_ms_hundredths = to_hundredths(metrics.current.ms),
        .average_fps_tenths = to_tenths(metrics.average.fps),
        .average_ms_hundredths = to_hundredths(metrics.average.ms),
        .low_0_1_fps_tenths = to_tenths(metrics.low_0_1pct.fps),
        .low_0_1_ms_hundredths = to_hundredths(metrics.low_0_1pct.ms),
        .low_1_fps_tenths = to_tenths(metrics.low_1pct.fps),
        .low_1_ms_hundredths = to_hundredths(metrics.low_1pct.ms),
        .low_x5_fps_tenths = to_tenths(metrics.confirmed_low_5.fps),
        .low_x5_ms_hundredths = to_hundredths(metrics.confirmed_low_5.ms),
        .low_x10_fps_tenths = to_tenths(metrics.confirmed_low_10.fps),
        .low_x10_ms_hundredths = to_hundredths(metrics.confirmed_low_10.ms),
        .worst_fps_tenths = to_tenths(metrics.worst.fps),
        .worst_ms_hundredths = to_hundredths(metrics.worst.ms),
        .warmup_elapsed_hundredths = to_hundredths(metrics.warmup_elapsed_seconds),
        .warmup_total_hundredths = to_hundredths(metrics.warmup_seconds),
        .sample_count = metrics.sample_count > 0xffffffffull ? 0xffffffffu : static_cast<Uint32>(metrics.sample_count),
        .warmup_complete = metrics.warmup_complete ? 1u : 0u,
        .low_x5_hits = metrics.confirmed_low_5_hits,
        .low_x10_hits = metrics.confirmed_low_10_hits,
    };
}

app_frame_metrics_snapshot_t main_frame_profile_metrics_snapshot(const main_frame_profile_t* state)
{
    if (!state)
    {
        return {};
    }
    return app_frame_metrics_snapshot(&state->frame_metrics, SDL_GetTicksNS());
}
