#include "app/runtime/shutdown.h"

#include "app/lighting/settings/settings.h"
#include "app/managed/managed_host.h"
#include "app/runtime/internal.h"
#include "app/runtime/stream_benchmark.h"
#include "app/world_time/clock.h"
#include "physics/jolt/jolt_physics_service.h"

void app_runtime_shutdown(void* appstate, SDL_AppResult result)
{
    (void) appstate;
    SDL_Log("SDL_AppQuit called with result=%d", (int) result);
    if (window)
    {
        SDL_HideWindow(window);
    }
    if (window && persistence_initialized)
    {
        main_runtime_settings_persist(&runtime_settings);
    }
    if (device)
    {
        SDL_WaitForGPUIdle(device);
    }
    app_managed_host_shutdown();
    octaryn::physics::jolt_physics_service_shutdown();
    if (world_initialized)
    {
        world_free();
        world_initialized = false;
    }
    if (persistence_initialized)
    {
        if (lighting_tuning_dirty)
        {
            main_lighting_settings_save(&lighting_tuning);
            lighting_tuning_dirty = false;
        }
        app_persist_world_clock();
        if (!app_stream_benchmark_active())
        {
            player_save_or_load(&player, PLAYER_ID, true);
        }
        persistence_free();
        persistence_initialized = false;
    }
    if (imgui_initialized)
    {
        main_imgui_lighting_shutdown();
        imgui_initialized = false;
    }
    if (resources_initialized)
    {
        main_render_resources_destroy(&resources, device);
        resources_initialized = false;
    }
    if (pipelines_initialized)
    {
        main_pipelines_destroy(&pipelines, device);
        pipelines_initialized = false;
    }
    if (audio_initialized)
    {
        oct_audio_shutdown();
        audio_initialized = false;
    }
    if (device && window && window_claimed_for_gpu)
    {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        window_claimed_for_gpu = false;
    }
    if (device)
    {
        SDL_DestroyGPUDevice(device);
        device = NULL;
    }
    if (window)
    {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}
