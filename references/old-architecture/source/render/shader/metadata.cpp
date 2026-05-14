#include "render/shader/internal.h"

#include <string_view>

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include "core/log.h"

namespace {

constexpr glz::opts k_glaze_json_opts{.null_terminated = false};

template <typename T>
bool parse_json_metadata(T& metadata, const char* path, const void* data, size_t size)
{
    std::string_view json{static_cast<const char*>(data), size};
    auto error = glz::read<k_glaze_json_opts>(metadata, json);
    if (error)
    {
        oct_log_errorf("Failed to parse shader json: %s", path);
        return false;
    }
    return true;
}

bool validate_graphics_metadata(const graphics_shader_metadata& metadata, const char* path)
{
    if (metadata.uniform_buffers > 4)
    {
        oct_log_errorf("Shader metadata exceeds SDL GPU graphics-stage uniform buffer limit (4): %s", path);
        return false;
    }
    return true;
}

bool validate_compute_metadata(const compute_shader_metadata& metadata, const char* path)
{
    if (metadata.uniform_buffers > 4)
    {
        oct_log_errorf("Shader metadata exceeds SDL GPU compute-stage uniform buffer limit (4): %s", path);
        return false;
    }
    return true;
}

} // namespace

void sdl_free_deleter::operator()(void* data) const
{
    SDL_free(data);
}

auto shader_load_file_internal(const char* path, size_t* size_out) -> sdl_memory_ptr
{
    void* data = SDL_LoadFile(path, size_out);
    if (!data)
    {
        oct_log_errorf("Failed to load file: %s", path);
    }
    return sdl_memory_ptr{data};
}

bool shader_load_graphics_metadata_internal(graphics_shader_metadata* metadata,
                                            const char* path,
                                            const void* data,
                                            size_t size)
{
    return parse_json_metadata(*metadata, path, data, size) && validate_graphics_metadata(*metadata, path);
}

bool shader_load_compute_metadata_internal(compute_shader_metadata* metadata,
                                           const char* path,
                                           const void* data,
                                           size_t size)
{
    return parse_json_metadata(*metadata, path, data, size) && validate_compute_metadata(*metadata, path);
}
