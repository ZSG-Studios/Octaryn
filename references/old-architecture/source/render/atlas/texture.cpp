#include "render/atlas/texture.h"

#include "render/atlas/upload/upload.h"
#include "core/asset_paths.h"
#include "core/profile.h"

namespace {

auto atlas_layer_count(const SDL_Surface* surface) -> Uint32
{
    return static_cast<Uint32>(surface->w / MAIN_RENDER_ATLAS_BLOCK_WIDTH);
}

auto load_atlas_surface(const char* asset_name, SDL_Surface** surface) -> bool
{
    char path[512] = {0};
    if (!asset_path_build(path, sizeof(path), asset_name))
    {
        SDL_Log("Failed to build atlas asset path for %s", asset_name);
        return false;
    }

    Uint64 step_start = oct_profile_now_ticks();
    *surface = SDL_LoadPNG(path);
    if (!*surface)
    {
        SDL_Log("Failed to create atlas surface %s: %s", asset_name, SDL_GetError());
        return false;
    }
    oct_profile_log_duration("Startup timing", "create_atlas | load atlas png", step_start);

    step_start = oct_profile_now_ticks();
    if ((*surface)->format != SDL_PIXELFORMAT_RGBA32)
    {
        SDL_Surface* converted = SDL_ConvertSurface(*surface, SDL_PIXELFORMAT_RGBA32);
        if (!converted)
        {
            SDL_Log("Failed to convert atlas surface: %s", SDL_GetError());
            return false;
        }
        SDL_DestroySurface(*surface);
        *surface = converted;
    }
    oct_profile_log_duration("Startup timing", "create_atlas | convert surface", step_start);
    return true;
}

auto validate_atlas_surface(const SDL_Surface* surface, const char* asset_name, int expected_width = 0) -> bool
{
    const int expected_atlas_width = MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_LAYER_COUNT;
    if (!surface || surface->h != MAIN_RENDER_ATLAS_BLOCK_WIDTH || surface->w != expected_atlas_width)
    {
        SDL_Log("Unexpected atlas dimensions for %s: %dx%d; expected %dx%d",
                asset_name,
                surface ? surface->w : 0,
                surface ? surface->h : 0,
                expected_atlas_width,
                MAIN_RENDER_ATLAS_BLOCK_WIDTH);
        return false;
    }
    if (expected_width > 0 && surface->w != expected_width)
    {
        SDL_Log("Unexpected atlas width for %s: %d; expected %d", asset_name, surface->w, expected_width);
        return false;
    }
    return true;
}

auto create_atlas_texture(SDL_GPUDevice* device,
                          SDL_GPUTexture** texture,
                          SDL_GPUTextureFormat format,
                          Uint32 layer_count,
                          Uint32 mip_levels,
                          const char* label) -> bool
{
    SDL_GPUTextureCreateInfo texture_info = {};
    texture_info.format = format;
    texture_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    texture_info.layer_count_or_depth = layer_count;
    texture_info.num_levels = mip_levels;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = static_cast<Uint32>(MAIN_RENDER_ATLAS_BLOCK_WIDTH);
    texture_info.height = static_cast<Uint32>(MAIN_RENDER_ATLAS_BLOCK_WIDTH);
    *texture = SDL_CreateGPUTexture(device, &texture_info);
    if (!*texture)
    {
        SDL_Log("Failed to create %s atlas texture: %s", label, SDL_GetError());
        return false;
    }

    return true;
}

} // namespace

auto main_render_atlas_mip_levels(void) -> Uint32
{
    Uint32 levels = 1;
    int size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    while (size > 1)
    {
        size >>= 1;
        levels += 1;
    }
    return levels;
}

auto main_render_atlas_create_texture(main_render_resources_t* resources, SDL_GPUDevice* device) -> bool
{
    OCT_PROFILE_ZONE("startup_create_atlas");
    const Uint64 total_start = oct_profile_now_ticks();

    if (!load_atlas_surface("textures/atlas.png", &resources->atlas_surface) ||
        !load_atlas_surface("textures/atlas_n.png", &resources->atlas_normal_surface) ||
        !load_atlas_surface("textures/atlas_s.png", &resources->atlas_specular_surface))
    {
        return false;
    }
    if (!validate_atlas_surface(resources->atlas_surface, "textures/atlas.png") ||
        !validate_atlas_surface(resources->atlas_normal_surface, "textures/atlas_n.png", resources->atlas_surface->w) ||
        !validate_atlas_surface(resources->atlas_specular_surface, "textures/atlas_s.png", resources->atlas_surface->w))
    {
        return false;
    }

    const Uint32 layer_count = atlas_layer_count(resources->atlas_surface);
    const Uint32 mip_levels = main_render_atlas_mip_levels();
    if (atlas_layer_count(resources->atlas_normal_surface) != layer_count ||
        atlas_layer_count(resources->atlas_specular_surface) != layer_count)
    {
        SDL_Log("PBR atlas layer counts do not match albedo atlas");
        return false;
    }

    if (!create_atlas_texture(device,
                              &resources->atlas_texture,
                              SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              layer_count,
                              mip_levels,
                              "albedo") ||
        !create_atlas_texture(device,
                              &resources->atlas_normal_texture,
                              SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                              layer_count,
                              mip_levels,
                              "normal") ||
        !create_atlas_texture(device,
                              &resources->atlas_specular_texture,
                              SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                              layer_count,
                              mip_levels,
                              "specular"))
    {
        return false;
    }

    if (!main_render_atlas_upload(resources, device, layer_count, mip_levels) ||
        !main_render_atlas_upload_texture(resources->atlas_normal_surface,
                                          resources->atlas_normal_texture,
                                          device,
                                          layer_count,
                                          mip_levels,
                                          MAIN_RENDER_ATLAS_MIP_LABPBR_NORMAL,
                                          "normal") ||
        !main_render_atlas_upload_texture(resources->atlas_specular_surface,
                                          resources->atlas_specular_texture,
                                          device,
                                          layer_count,
                                          mip_levels,
                                          MAIN_RENDER_ATLAS_MIP_LABPBR_SPECULAR,
                                          "specular"))
    {
        return false;
    }

    oct_profile_log_duration("Startup timing", "create_atlas", total_start);
    return true;
}
