#include "app/runtime/startup/runtime.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/render/present.h"
#include "app/runtime/telemetry.h"
#include "core/log.h"

bool app_startup_init_runtime_subsystems(void)
{
    Uint64 startup_step = app_profile_now();
    oct_audio_init();
    audio_initialized = true;
    oct_log_infof("Startup timing | oct_audio_init took %.2f ms", app_profile_elapsed_ms(startup_step));

    app_frame_pacing_init(&frame_pacing);

    startup_step = app_profile_now();
    if (!main_window_configure_swapchain(&main_window, device, window, &frame_pacing))
    {
        return false;
    }
    oct_log_infof("Startup timing | main_window_configure_swapchain took %.2f ms", app_profile_elapsed_ms(startup_step));
    const SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);

    startup_step = app_profile_now();
    imgui_initialized = main_imgui_lighting_init(window, device, swapchain_format);
    oct_log_infof("Startup timing | main_imgui_lighting_init took %.2f ms", app_profile_elapsed_ms(startup_step));

    color_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    startup_step = app_profile_now();
    if (!main_render_resources_init(&resources, device, color_format, depth_format, swapchain_format))
    {
        SDL_Log("Failed to initialize render resources: %s", SDL_GetError());
        return false;
    }
    resources_initialized = true;
    oct_log_infof("Startup timing | main_render_resources_init call took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    if (!main_pipelines_init(&pipelines, device, color_format, depth_format, swapchain_format))
    {
        SDL_Log("Failed to initialize render pipelines");
        return false;
    }
    pipelines_initialized = true;
    oct_log_infof("Startup timing | main_pipelines_init call took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    app_runtime_telemetry_init();
    oct_log_infof("Startup timing | app_runtime_telemetry_init took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    if (!main_window_show(window))
    {
        return false;
    }
    SDL_Log("Window shown successfully");
    oct_log_infof("Startup timing | main_window_show took %.2f ms", app_profile_elapsed_ms(startup_step));
    startup_step = app_profile_now();
    app_present_startup_frame();
    oct_log_infof("Startup timing | app_present_startup_frame took %.2f ms", app_profile_elapsed_ms(startup_step));
    startup_step = app_profile_now();
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    oct_log_infof("Startup timing | SDL_FlashWindow took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    main_render_resources_set_window_icon(&resources, window, BLOCK_GRASS);
    oct_log_infof("Startup timing | main_render_resources_set_window_icon took %.2f ms",
                  app_profile_elapsed_ms(startup_step));
    return true;
}
