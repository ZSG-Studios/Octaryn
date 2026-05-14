#include "app/runtime/startup/session.h"

#include "app/overlay/display_menu.h"
#include "app/managed/managed_host.h"
#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/startup/runtime.h"
#include "app/startup_terrain/terrain.h"
#include "app/world_time/clock.h"
#include "core/log.h"
#include "physics/jolt/jolt_physics_service.h"

bool app_startup_init_session(void)
{
    Uint64 startup_step = app_profile_now();
    if (!persistence_init(runtime_options.save_path.c_str()))
    {
        SDL_Log("Failed to initialize save root: %s", runtime_options.save_path.c_str());
        return false;
    }
    persistence_initialized = true;
    oct_log_infof("Startup timing | persistence_init call took %.2f ms", app_profile_elapsed_ms(startup_step));

    main_runtime_settings_init(&runtime_settings, window, &fog_enabled, &clouds_enabled, &sky_gradient_enabled,
                               &stars_enabled, &sun_enabled, &moon_enabled, &pom_enabled, &pbr_enabled,
                               &startup_render_distance, &worldgen_worker_count);
    main_runtime_settings_refresh_worker_options(WORKER_OPTIONS, &WORKER_OPTION_COUNT, SDL_arraysize(WORKER_OPTIONS));

    startup_step = app_profile_now();
    main_runtime_settings_load(&runtime_settings);
    oct_log_infof("Startup timing | main_runtime_settings_load took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    if (main_lighting_settings_load(&lighting_tuning))
    {
        main_lighting_settings_save(&lighting_tuning);
        oct_log_infof("Loaded lighting tuning: ambient=%.2f sun=%.2f",
                      lighting_tuning.ambient_strength,
                      lighting_tuning.sun_strength);
    }

    persistence_world_save_metadata_t world_save_metadata = {};
    world_loaded_from_save = persistence_get_world_save_metadata(&world_save_metadata) && world_save_metadata.save_exists;
    if (!world_loaded_from_save)
    {
        lighting_tuning = main_lighting_settings_default();
        main_lighting_settings_save(&lighting_tuning);
        oct_log_infof("Reset lighting tuning to defaults for a fresh world");
    }
    oct_log_infof("Startup timing | main_lighting_settings_load took %.2f ms", app_profile_elapsed_ms(startup_step));

    app_prepare_startup_terrain();

    startup_step = app_profile_now();
    if (!app_startup_init_runtime_subsystems())
    {
        return false;
    }
    oct_log_infof("Startup timing | app_startup_init_runtime_subsystems took %.2f ms",
                  app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    world_init(device);
    world_initialized = true;
    oct_log_infof("Startup timing | world_init call took %.2f ms", app_profile_elapsed_ms(startup_step));
    world_set_generation_worker_count(worldgen_worker_count);

    startup_step = app_profile_now();
    if (!octaryn::physics::jolt_physics_service_startup())
    {
        return false;
    }
    oct_log_infof("Startup timing | jolt_physics_service_startup took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    player_loaded_from_save = player_save_or_load(&player, PLAYER_ID, false);
    oct_log_infof("Startup timing | player_save_or_load(load) took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    app_init_world_clock();
    oct_log_infof("Startup timing | init_world_clock took %.2f ms", app_profile_elapsed_ms(startup_step));

    app_reset_startup_terrain_state();
    restore_relative_mouse_after_ui = false;
    quit_requested = false;
    main_frame_profile_init(&frame_profile);

    startup_step = app_profile_now();
    world_set_render_distance(&player.camera, startup_bootstrap_render_distance);
    world_update(&player.camera);
    app_maybe_align_spawn_to_surface();
    player_update_query(&player);
    oct_log_infof("Startup timing | initial world bootstrap took %.2f ms", app_profile_elapsed_ms(startup_step));

    app_open_display_menu();
    ticks2 = SDL_GetTicksNS();
    ticks1 = ticks2;
    SDL_SetWindowTitle(window, "Octaryn Engine");
    if (!app_managed_host_startup())
    {
        return false;
    }
    return true;
}
