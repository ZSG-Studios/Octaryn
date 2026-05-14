#include "render/atlas/internal.h"

#include "render/atlas/texture.h"
#include "core/profile.h"

namespace {

auto make_atlas_sampler_info(SDL_GPUFilter filter, SDL_GPUSamplerMipmapMode mipmap_mode) -> SDL_GPUSamplerCreateInfo
{
    SDL_GPUSamplerCreateInfo info = {};
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = mipmap_mode;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.enable_compare = false;
    info.max_lod = static_cast<float>(main_render_atlas_mip_levels() - 1);
    return info;
}

} // namespace

auto main_render_atlas_create_samplers(main_render_resources_t* resources, SDL_GPUDevice* device) -> bool
{
    OCT_PROFILE_ZONE("startup_create_samplers");
    const Uint64 start_ticks = oct_profile_now_ticks();

    SDL_GPUSamplerCreateInfo atlas_info =
        make_atlas_sampler_info(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR);
    atlas_info.enable_anisotropy = true;
    atlas_info.max_anisotropy = 8.0f;
    resources->atlas_sampler = SDL_CreateGPUSampler(device, &atlas_info);
    if (!resources->atlas_sampler)
    {
        SDL_Log("Falling back to non-anisotropic atlas sampler: %s", SDL_GetError());
        atlas_info.enable_anisotropy = false;
        atlas_info.max_anisotropy = 1.0f;
        resources->atlas_sampler = SDL_CreateGPUSampler(device, &atlas_info);
    }
    if (!resources->atlas_sampler)
    {
        SDL_Log("Failed to create atlas sampler: %s", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo cutout_info =
        make_atlas_sampler_info(SDL_GPU_FILTER_NEAREST, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST);
    resources->cutout_sampler = SDL_CreateGPUSampler(device, &cutout_info);
    if (!resources->cutout_sampler)
    {
        SDL_Log("Failed to create cutout sampler: %s", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo nearest_info =
        make_atlas_sampler_info(SDL_GPU_FILTER_NEAREST, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST);
    nearest_info.max_lod = 0.0f;
    resources->nearest_sampler = SDL_CreateGPUSampler(device, &nearest_info);
    if (!resources->nearest_sampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }

    oct_profile_log_duration("Startup timing", "create_samplers", start_ticks);
    return true;
}
