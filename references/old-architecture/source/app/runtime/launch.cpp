#include "app/runtime/launch.h"

#include "app/runtime/internal.h"

void app_configure_launch_mode(void)
{
    gpu_validation_enabled = false;
    gpu_validation_unavailable = false;
    detached_launch_mode = false;
#ifndef NDEBUG
    const char* gpu_validation_env = SDL_getenv("OCTARYN_ENABLE_GPU_VALIDATION");
    gpu_validation_enabled = gpu_validation_env && SDL_strcmp(gpu_validation_env, "0") != 0;
#endif
    if (gpu_validation_enabled)
    {
        SDL_Log("GPU validation enabled via OCTARYN_ENABLE_GPU_VALIDATION");
    }
    const char* detached_launch_env = SDL_getenv("OCTARYN_DETACHED_LAUNCH");
    detached_launch_mode = detached_launch_env && SDL_strcmp(detached_launch_env, "0") != 0;
    startup_ticks_ns = SDL_GetTicksNS();
}

bool app_should_ignore_early_close(void)
{
    return detached_launch_mode && (SDL_GetTicksNS() - startup_ticks_ns) < 5000000000ULL;
}
