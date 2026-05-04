#include "ShaderCreation.h"

#include <cstdint>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SHADER_CREATION_USE_SDL3)
auto create_graphics_shader(
    SDL_GPUDevice* device,
    const GraphicsShaderMetadata& metadata,
    const void* code,
    std::size_t code_size,
    const char* entrypoint,
    SDL_GPUShaderFormat format,
    SDL_GPUShaderStage stage) -> SDL_GPUShader*
{
    if (device == nullptr || code == nullptr || code_size == 0u || entrypoint == nullptr ||
        !validate_graphics_shader_metadata(metadata))
    {
        return nullptr;
    }

    SDL_GPUShaderCreateInfo info = {};
    info.num_samplers = metadata.samplers;
    info.num_storage_textures = metadata.storageTextures;
    info.num_storage_buffers = metadata.storageBuffers;
    info.num_uniform_buffers = metadata.uniformBuffers;
    info.code = static_cast<const std::uint8_t*>(code);
    info.code_size = code_size;
    info.entrypoint = entrypoint;
    info.format = format;
    info.stage = stage;
    return SDL_CreateGPUShader(device, &info);
}

auto create_compute_pipeline(
    SDL_GPUDevice* device,
    const ComputeShaderMetadata& metadata,
    const void* code,
    std::size_t code_size,
    const char* entrypoint,
    SDL_GPUShaderFormat format) -> SDL_GPUComputePipeline*
{
    if (device == nullptr || code == nullptr || code_size == 0u || entrypoint == nullptr ||
        !validate_compute_shader_metadata(metadata))
    {
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo info = {};
    info.num_samplers = metadata.samplers;
    info.num_readonly_storage_textures = metadata.readonlyStorageTextures;
    info.num_readonly_storage_buffers = metadata.readonlyStorageBuffers;
    info.num_readwrite_storage_textures = metadata.readwriteStorageTextures;
    info.num_readwrite_storage_buffers = metadata.readwriteStorageBuffers;
    info.num_uniform_buffers = metadata.uniformBuffers;
    info.threadcount_x = metadata.threadcountX;
    info.threadcount_y = metadata.threadcountY;
    info.threadcount_z = metadata.threadcountZ;
    info.code = static_cast<const std::uint8_t*>(code);
    info.code_size = code_size;
    info.entrypoint = entrypoint;
    info.format = format;
    return SDL_CreateGPUComputePipeline(device, &info);
}
#endif

} // namespace octaryn::client::rendering
