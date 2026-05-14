#include "render/resources/window_textures.h"

#include <cstdlib>

namespace
{
void copy_window_texture_formats(main_render_resources_t* destination, const main_render_resources_t* source)
{
    destination->color_format = source->color_format;
    destination->depth_format = source->depth_format;
    destination->present_format = source->present_format;
}

bool env_float_value(const char* name, float* out_value)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value)
    {
        return false;
    }
    *out_value = parsed;
    return true;
}

float select_render_scale()
{
    float requested_scale = 0.0f;
    if (env_float_value("OCTARYN_RENDER_SCALE", &requested_scale))
    {
        return SDL_clamp(requested_scale, 0.25f, 2.0f);
    }

    return 1.0f;
}

void select_render_extent(int window_width, int window_height, Uint32* out_width, Uint32* out_height)
{
    const float scale = select_render_scale();
    *out_width = static_cast<Uint32>(SDL_max(1, static_cast<int>(static_cast<float>(window_width) * scale + 0.5f)));
    *out_height = static_cast<Uint32>(SDL_max(1, static_cast<int>(static_cast<float>(window_height) * scale + 0.5f)));
}

void move_window_textures(main_render_resources_t* destination, main_render_resources_t* source)
{
    destination->depth_texture = source->depth_texture;
    destination->color_texture = source->color_texture;
    destination->voxel_texture = source->voxel_texture;
    destination->position_texture = source->position_texture;
    destination->composite_texture = source->composite_texture;
    destination->imgui_texture = source->imgui_texture;
    destination->material_texture = source->material_texture;
    destination->window_width = source->window_width;
    destination->window_height = source->window_height;
    destination->render_width = source->render_width;
    destination->render_height = source->render_height;

    source->depth_texture = NULL;
    source->color_texture = NULL;
    source->voxel_texture = NULL;
    source->position_texture = NULL;
    source->composite_texture = NULL;
    source->imgui_texture = NULL;
    source->material_texture = NULL;
    source->window_width = 0;
    source->window_height = 0;
    source->render_width = 0;
    source->render_height = 0;
}

bool create_window_textures(main_render_resources_t* resources, SDL_GPUDevice* device, int window_width, int window_height)
{
    Uint32 render_width = 0;
    Uint32 render_height = 0;
    select_render_extent(window_width, window_height, &render_width, &render_height);

    resources->window_width = static_cast<Uint32>(SDL_max(window_width, 1));
    resources->window_height = static_cast<Uint32>(SDL_max(window_height, 1));
    resources->render_width = render_width;
    resources->render_height = render_height;

    SDL_GPUTextureCreateInfo info = {};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = resources->depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = render_width;
    info.height = render_height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    resources->depth_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    info.format = resources->color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->color_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->color_texture)
    {
        SDL_Log("Failed to create color texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->voxel_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->voxel_texture)
    {
        SDL_Log("Failed to create voxel texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->position_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->material_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->material_texture)
    {
        SDL_Log("Failed to create material texture: %s", SDL_GetError());
        return false;
    }
    info.width = render_width;
    info.height = render_height;
    info.format = resources->color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
               | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
               | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE
               | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->composite_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->composite_texture)
    {
        SDL_Log("Failed to create composite texture: %s", SDL_GetError());
        return false;
    }
    info.format = resources->present_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resources->imgui_texture = SDL_CreateGPUTexture(device, &info);
    if (!resources->imgui_texture)
    {
        SDL_Log("Failed to create imgui texture: %s", SDL_GetError());
        return false;
    }
    if (resources->render_width != resources->window_width || resources->render_height != resources->window_height)
    {
        SDL_Log("Internal render resolution: %ux%u for %ux%u swapchain",
                resources->render_width,
                resources->render_height,
                resources->window_width,
                resources->window_height);
    }
    return true;
}
}

void main_render_resources_release_window_textures(main_render_resources_t* resources, SDL_GPUDevice* device)
{
    if (resources->depth_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->depth_texture);
    }
    if (resources->color_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->color_texture);
    }
    if (resources->voxel_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->voxel_texture);
    }
    if (resources->position_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->position_texture);
    }
    if (resources->composite_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->composite_texture);
    }
    if (resources->imgui_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->imgui_texture);
    }
    if (resources->material_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->material_texture);
    }
    resources->depth_texture = NULL;
    resources->color_texture = NULL;
    resources->voxel_texture = NULL;
    resources->position_texture = NULL;
    resources->composite_texture = NULL;
    resources->imgui_texture = NULL;
    resources->material_texture = NULL;
    resources->window_width = 0;
    resources->window_height = 0;
    resources->render_width = 0;
    resources->render_height = 0;
}

bool main_render_resources_resize(main_render_resources_t* resources, SDL_GPUDevice* device, int width, int height)
{
    main_render_resources_t pending = {};
    copy_window_texture_formats(&pending, resources);

    if (!create_window_textures(&pending, device, width, height))
    {
        main_render_resources_release_window_textures(&pending, device);
        return false;
    }

    main_render_resources_release_window_textures(resources, device);
    move_window_textures(resources, &pending);
    return true;
}
