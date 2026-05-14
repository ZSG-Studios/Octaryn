#include "app/runtime/iterate.h"

#include "app/overlay/display_menu.h"
#include "app/managed/managed_frame_snapshot.h"
#include "app/managed/managed_host.h"
#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/render/render.h"
#include "app/runtime/renderdoc_capture.h"
#include "app/runtime/stream_benchmark.h"
#include "app/startup_terrain/terrain.h"
#include "app/world_time/clock.h"

#include "core/log.h"
#include "core/profile.h"
#include "physics/jolt/jolt_physics_service.h"

bool g_running = true;

namespace {

Uint64 g_last_iterate_end_ticks = 0;
std::uint64_t g_managed_frame_index = 0;

} // namespace

SDL_AppResult app_runtime_iterate(void* appstate)
{
    (void) appstate;
    OCT_PROFILE_ZONE("SDL_AppIterate");

    const Uint64 frame_start = app_profile_now();
    main_frame_profile_sample_t frame_profile_sample = {};
    main_frame_profile_note_frame_start(&frame_profile_sample, frame_start);
    if (g_last_iterate_end_ticks != 0u && frame_start > g_last_iterate_end_ticks)
    {
        frame_profile_sample.app_callback_gap_ms =
            static_cast<float>(frame_start - g_last_iterate_end_ticks) * 1e-6f;
    }
    Uint64 misc_start = app_profile_now();

    main_display_menu_commit_if_requested(&display_menu, app_get_display_menu_runtime());
    app_stream_benchmark_init();
    app_renderdoc_capture_init();
    ticks2 = SDL_GetTicksNS();
    const float dt = (float) (ticks2 - ticks1) * 1e-6f;
    ticks1 = ticks2;
    const double world_clock_dt = SDL_min(dt * 0.001, 0.25);
    world_time_advance_real_seconds(&world_clock, world_clock_dt);
    app_refresh_world_clock_snapshot();
    app_mark_world_clock_dirty();
    app_persist_world_clock_if_due();
    frame_profile_sample.misc_ms += app_profile_elapsed_ms(misc_start);

    misc_start = app_profile_now();
    main_window_update_title(&main_window, window, APP_NAME, ticks2, dt);
    frame_profile_sample.misc_ms += app_profile_elapsed_ms(misc_start);

    if (SDL_GetWindowRelativeMouseMode(window))
    {
        const Uint64 sim_start = app_profile_now();
        player_move(&player, dt);
        if (!app_stream_benchmark_active())
        {
            player_save_if_due(&player, PLAYER_ID);
        }
        frame_profile_sample.sim_ms += app_profile_elapsed_ms(sim_start);
    }
    misc_start = app_profile_now();
    app_stream_benchmark_step(&player, dt);
    const oct_managed_frame_snapshot_t managed_frame =
        app_managed_frame_snapshot_build(&player, &world_clock_snapshot, world_clock_dt, g_managed_frame_index++);
    app_managed_host_tick(&managed_frame);
    app_managed_host_drain_commands();
    octaryn::physics::jolt_physics_service_tick(world_clock_dt);
    frame_profile_sample.misc_ms += app_profile_elapsed_ms(misc_start);
    if (quit_requested)
    {
        return SDL_APP_SUCCESS;
    }

    const Uint64 world_update_start = app_profile_now();
    world_update(&player.camera);
    frame_profile_sample.world_ms = app_profile_elapsed_ms(world_update_start);

    misc_start = app_profile_now();
    app_maybe_align_spawn_to_surface();
    frame_profile_sample.misc_ms += app_profile_elapsed_ms(misc_start);

    misc_start = app_profile_now();
    player_update_query(&player);
    app_maybe_log_terrain_debug();
    app_maybe_expand_startup_render_distance();
    if (!first_frame_logged)
    {
        SDL_Log("Submitting first runtime frame");
        first_frame_logged = true;
        main_window_finish_show(window);
    }
    camera_update(&player.camera);
    frame_profile_sample.misc_ms += app_profile_elapsed_ms(misc_start);
    app_renderdoc_capture_before_render();
    app_render(&frame_profile_sample);

    frame_profile_sample.fps_cap_sleep_ms = app_frame_pacing_sleep_until_next_frame(&frame_pacing, frame_start);
    const Uint64 frame_end = app_profile_now();
    main_frame_profile_note_frame_end(&frame_profile_sample, frame_end);
    frame_profile_sample.total_ms = static_cast<float>(frame_end - frame_start) * 1e-6f;
    main_frame_profile_finalize_sample(&frame_profile_sample);
    if (frame_profile_sample.present_submitted)
    {
        OCT_PROFILE_FRAME();
    }
    main_frame_profile_record(&frame_profile, &frame_profile_sample);
    app_maybe_log_profile_spike(&frame_profile_sample);
    g_last_iterate_end_ticks = app_profile_now();
    return SDL_APP_CONTINUE;
}
