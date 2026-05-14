#include "render/shader/internal.h"

#include <SDL3/SDL.h>

void* shader_create_graphics_internal(SDL_GPUDevice* device,
                                      const graphics_shader_metadata& metadata,
                                      const void* code,
                                      size_t code_size,
                                      const char* entrypoint,
                                      SDL_GPUShaderFormat format,
                                      SDL_GPUShaderStage stage)
{
    SDL_GPUShaderCreateInfo info = {};
    info.num_samplers = metadata.samplers;
    info.num_storage_textures = metadata.storage_textures;
    info.num_storage_buffers = metadata.storage_buffers;
    info.num_uniform_buffers = metadata.uniform_buffers;
    info.code = static_cast<const Uint8*>(code);
    info.code_size = code_size;
    info.entrypoint = entrypoint;
    info.format = format;
    info.stage = stage;
    return SDL_CreateGPUShader(device, &info);
}

void* shader_create_compute_internal(SDL_GPUDevice* device,
                                     const compute_shader_metadata& metadata,
                                     const void* code,
                                     size_t code_size,
                                     const char* entrypoint,
                                     SDL_GPUShaderFormat format)
{
    SDL_GPUComputePipelineCreateInfo info = {};
    info.num_samplers = metadata.samplers;
    info.num_readonly_storage_textures = metadata.readonly_storage_textures;
    info.num_readonly_storage_buffers = metadata.readonly_storage_buffers;
    info.num_readwrite_storage_textures = metadata.readwrite_storage_textures;
    info.num_readwrite_storage_buffers = metadata.readwrite_storage_buffers;
    info.num_uniform_buffers = metadata.uniform_buffers;
    info.threadcount_x = metadata.threadcount_x;
    info.threadcount_y = metadata.threadcount_y;
    info.threadcount_z = metadata.threadcount_z;
    info.code = static_cast<const Uint8*>(code);
    info.code_size = code_size;
    info.entrypoint = entrypoint;
    info.format = format;
    return SDL_CreateGPUComputePipeline(device, &info);
}
