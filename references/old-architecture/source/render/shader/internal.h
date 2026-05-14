#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include "render/shader/asset_metadata.hpp"

struct sdl_free_deleter
{
    void operator()(void* data) const;
};

using sdl_memory_ptr = std::unique_ptr<void, sdl_free_deleter>;

auto shader_load_file_internal(const char* path, size_t* size_out) -> sdl_memory_ptr;
bool shader_load_graphics_metadata_internal(graphics_shader_metadata* metadata,
                                            const char* path,
                                            const void* data,
                                            size_t size);
bool shader_load_compute_metadata_internal(compute_shader_metadata* metadata,
                                           const char* path,
                                           const void* data,
                                           size_t size);
void* shader_create_graphics_internal(SDL_GPUDevice* device,
                                      const graphics_shader_metadata& metadata,
                                      const void* code,
                                      size_t code_size,
                                      const char* entrypoint,
                                      SDL_GPUShaderFormat format,
                                      SDL_GPUShaderStage stage);
void* shader_create_compute_internal(SDL_GPUDevice* device,
                                     const compute_shader_metadata& metadata,
                                     const void* code,
                                     size_t code_size,
                                     const char* entrypoint,
                                     SDL_GPUShaderFormat format);
