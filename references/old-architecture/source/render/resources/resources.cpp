#include "render/resources/resources.h"

#include "render/atlas/atlas.h"
#include "render/resources/window_textures.h"
#include "core/profile.h"

bool main_render_resources_init(main_render_resources_t* resources, SDL_GPUDevice* device,
                                SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format,
                                SDL_GPUTextureFormat present_format)
{
    OCT_PROFILE_ZONE("main_render_resources_init");
    const Uint64 start_ticks = oct_profile_now_ticks();
    *resources = {};
    resources->color_format = color_format;
    resources->depth_format = depth_format;
    resources->present_format = present_format;
    if (!main_render_atlas_init(resources, device))
    {
        main_render_resources_destroy(resources, device);
        return false;
    }
    oct_profile_log_duration("Startup timing", "main_render_resources_init", start_ticks);
    return true;
}

void main_render_resources_set_window_icon(const main_render_resources_t* resources, SDL_Window* window, block_t block)
{
    main_render_atlas_set_window_icon(resources, window, block);
}

void main_render_resources_destroy(main_render_resources_t* resources, SDL_GPUDevice* device)
{
    main_render_resources_release_window_textures(resources, device);
    main_render_atlas_destroy(resources, device);
    *resources = {};
}
