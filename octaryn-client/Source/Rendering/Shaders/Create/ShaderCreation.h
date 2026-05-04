#pragma once

#include <cstddef>

#include "ShaderMetadataContract.hpp"

#if defined(OCTARYN_CLIENT_SHADER_CREATION_USE_SDL3)
#include <SDL3/SDL_gpu.h>
#endif

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SHADER_CREATION_USE_SDL3)
auto create_graphics_shader(
    SDL_GPUDevice* device,
    const GraphicsShaderMetadata& metadata,
    const void* code,
    std::size_t code_size,
    const char* entrypoint,
    SDL_GPUShaderFormat format,
    SDL_GPUShaderStage stage) -> SDL_GPUShader*;

auto create_compute_pipeline(
    SDL_GPUDevice* device,
    const ComputeShaderMetadata& metadata,
    const void* code,
    std::size_t code_size,
    const char* entrypoint,
    SDL_GPUShaderFormat format) -> SDL_GPUComputePipeline*;
#endif

} // namespace octaryn::client::rendering
