#include <SDL3/SDL.h>

#include "core/asset_paths.h"
#include "core/check.h"
#include "core/log.h"
#include "core/profile.h"
#include "render/shader/shader.h"
#include "render/shader/internal.h"

void* shader_load(SDL_GPUDevice* device, const char* name)
{
    OCT_PROFILE_ZONE("shader_load");
    const Uint64 start_ticks = oct_profile_now_ticks();
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(device);
    const char* entrypoint = nullptr;
    const char* file_extension = nullptr;
    if (format & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
        file_extension = "spv";
    }
    else if (format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
        file_extension = "dxil";
    }
    else if (format & SDL_GPU_SHADERFORMAT_MSL)
    {
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
        file_extension = "msl";
    }
    else
    {
        CHECK(false);
        return nullptr;
    }

    char shader_relative_path[256] = {0};
    char shader_json_relative_path[256] = {0};
    char shader_path[512] = {0};
    char shader_json_path[512] = {0};
    SDL_snprintf(shader_relative_path, sizeof(shader_relative_path), "shaders/%s.%s", name, file_extension);
    SDL_snprintf(shader_json_relative_path, sizeof(shader_json_relative_path), "shaders/%s.json", name);
    if (!asset_path_build(shader_path, sizeof(shader_path), shader_relative_path) ||
        !asset_path_build(shader_json_path, sizeof(shader_json_path), shader_json_relative_path))
    {
        oct_log_errorf("Failed to build shader asset paths for: %s", name);
        return nullptr;
    }

    size_t shader_size = 0;
    size_t shader_json_size = 0;
    sdl_memory_ptr shader_data = shader_load_file_internal(shader_path, &shader_size);
    if (!shader_data)
    {
        return nullptr;
    }
    sdl_memory_ptr shader_json_data = shader_load_file_internal(shader_json_path, &shader_json_size);
    if (!shader_json_data)
    {
        return nullptr;
    }

    void* shader = nullptr;
    if (SDL_strstr(name, ".comp"))
    {
        compute_shader_metadata metadata{};
        if (!shader_load_compute_metadata_internal(&metadata, shader_json_path, shader_json_data.get(), shader_json_size))
        {
            return nullptr;
        }
        shader = shader_create_compute_internal(device,
                                               metadata,
                                               shader_data.get(),
                                               shader_size,
                                               entrypoint,
                                               format);
    }
    else
    {
        graphics_shader_metadata metadata{};
        if (!shader_load_graphics_metadata_internal(&metadata, shader_json_path, shader_json_data.get(), shader_json_size))
        {
            return nullptr;
        }
        const SDL_GPUShaderStage stage =
          SDL_strstr(name, ".frag") ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
        shader = shader_create_graphics_internal(device,
                                                 metadata,
                                                 shader_data.get(),
                                                 shader_size,
                                                 entrypoint,
                                                 format,
                                                 stage);
    }

    if (!shader)
    {
        oct_log_errorf("Failed to create shader: %s", SDL_GetError());
        return nullptr;
    }
    oct_log_infof("Startup timing | shader_load(%s) took %.2f ms", name, oct_profile_elapsed_ms(start_ticks));
    return shader;
}
