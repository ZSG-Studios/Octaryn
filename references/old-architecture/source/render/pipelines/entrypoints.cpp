#include "render/pipelines/entrypoints_internal.h"

#include "render/pipelines/lifecycle_internal.h"

#include "core/profile.h"

bool main_pipelines_run_init(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                             SDL_GPUTextureFormat depth_format,
                             SDL_GPUTextureFormat swapchain_format)
{
    OCT_PROFILE_ZONE("main_pipelines_init");
    const Uint64 total_start = oct_profile_now_ticks();
    *pipelines = {};
    if (!main_pipelines_init_graphics(pipelines, device, color_format, depth_format, swapchain_format) ||
        !main_pipelines_init_compute(pipelines, device))
    {
        main_pipelines_run_destroy(pipelines, device);
        return false;
    }
    oct_profile_log_duration("Startup timing", "main_pipelines_init", total_start);
    return true;
}

void main_pipelines_run_destroy(main_pipelines_t* pipelines, SDL_GPUDevice* device)
{
    main_pipelines_release_compute(pipelines, device);
    main_pipelines_release_graphics(pipelines, device);
    *pipelines = {};
}
