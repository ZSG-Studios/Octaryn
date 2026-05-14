#include "app/runtime/startup/platform.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "core/log.h"

bool app_startup_init_platform(void)
{
    Uint64 startup_step = app_profile_now();
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    oct_log_infof("Startup timing | SDL_Init took %.2f ms", app_profile_elapsed_ms(startup_step));
    oct_log_infof("Using SDL video driver: %s", SDL_GetCurrentVideoDriver());

    startup_step = app_profile_now();
    window = SDL_CreateWindow(APP_NAME, 960, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }
    oct_log_infof("Startup timing | SDL_CreateWindow took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, gpu_validation_enabled, "vulkan");
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (gpu_validation_enabled && gpu_validation_unavailable)
    {
        oct_log_errorf("GPU validation was requested but Vulkan validation layers are unavailable; aborting startup");
        SDL_DestroyGPUDevice(device);
        device = nullptr;
        return false;
    }
    SDL_Log("Using SDL GPU driver: %s", SDL_GetGPUDeviceDriver(device));
    oct_log_infof("Startup timing | SDL_CreateGPUDevice took %.2f ms", app_profile_elapsed_ms(startup_step));

    startup_step = app_profile_now();
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        return false;
    }
    window_claimed_for_gpu = true;
    main_window_init(&main_window);
    oct_log_infof("Startup timing | SDL_ClaimWindowForGPUDevice took %.2f ms", app_profile_elapsed_ms(startup_step));
    return true;
}
