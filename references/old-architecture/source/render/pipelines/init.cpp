#include "render/pipelines/lifecycle_internal.h"

#include "core/profile.h"

namespace
{
void main_pipelines_log_step(const char* label, Uint64 start_ticks)
{
    oct_profile_log_duration("Startup timing", label, start_ticks);
}
}

bool main_pipelines_init_graphics(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format,
                                  SDL_GPUTextureFormat swapchain_format)
{
    Uint64 step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_opaque(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_opaque_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_opaque_sprite(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_opaque_sprite_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_transparent(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_transparent_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_water(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_water_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_lava(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_lava_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_depth(pipelines, device, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_depth_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_sky(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_sky_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_clouds(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_clouds_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_selection(pipelines, device, color_format, depth_format))
    {
        return false;
    }
    main_pipelines_log_step("create_selection_pipeline", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_create_present(pipelines, device, swapchain_format))
    {
        return false;
    }
    main_pipelines_log_step("create_present_pipeline", step_start);
    return true;
}

bool main_pipelines_init_compute(main_pipelines_t* pipelines, SDL_GPUDevice* device)
{
    Uint64 step_start = oct_profile_now_ticks();
    if (!main_pipelines_load_compute(&pipelines->ui, device, "ui.comp"))
    {
        return false;
    }
    main_pipelines_log_step("load_compute_pipeline(ui.comp)", step_start);

    step_start = oct_profile_now_ticks();
    if (!main_pipelines_load_compute(&pipelines->composite, device, "composite.comp"))
    {
        return false;
    }
    main_pipelines_log_step("load_compute_pipeline(composite.comp)", step_start);
    return true;
}
