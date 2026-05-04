#include "AssetPath.h"

#include <cstdio>
#include <cstring>

#if defined(OCTARYN_CLIENT_ASSET_PATHS_USE_SDL3)
#include <SDL3/SDL.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr const char* AssetRoot = "Assets/";

bool safe_bundle_relative_path(const char* relative_path)
{
    if (relative_path == nullptr || relative_path[0] == '\0' || relative_path[0] == '/')
    {
        return false;
    }

    const char* segment = relative_path;
    for (const char* cursor = relative_path;; ++cursor)
    {
        const char value = *cursor;
        if (value == '\\' || value == ':')
        {
            return false;
        }

        if (value == '/' || value == '\0')
        {
            const size_t length = static_cast<size_t>(cursor - segment);
            if ((length == 1u && segment[0] == '.') ||
                (length == 2u && segment[0] == '.' && segment[1] == '.'))
            {
                return false;
            }

            if (value == '\0')
            {
                return true;
            }

            segment = cursor + 1;
        }
    }
}

bool executable_directory(char* output, size_t output_size)
{
    if (output == nullptr || output_size == 0)
    {
        return false;
    }

#if defined(OCTARYN_CLIENT_ASSET_PATHS_USE_SDL3)
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr)
    {
        return false;
    }

    const int written = std::snprintf(output, output_size, "%s", base_path);
    return written > 0 && static_cast<size_t>(written) < output_size;
#elif defined(_WIN32)
    const DWORD written = GetModuleFileNameA(nullptr, output, static_cast<DWORD>(output_size));
    if (written == 0 || static_cast<size_t>(written) >= output_size)
    {
        return false;
    }
#else
    const ssize_t written = readlink("/proc/self/exe", output, output_size - 1u);
    if (written <= 0 || static_cast<size_t>(written) >= output_size)
    {
        return false;
    }

    output[written] = '\0';
#endif

#if !defined(OCTARYN_CLIENT_ASSET_PATHS_USE_SDL3)
    char* last_separator = std::strrchr(output, '/');
#if defined(_WIN32)
    char* last_backslash = std::strrchr(output, '\\');
    if (last_backslash != nullptr && (last_separator == nullptr || last_backslash > last_separator))
    {
        last_separator = last_backslash;
    }
#endif

    if (last_separator == nullptr)
    {
        return false;
    }

    last_separator[1] = '\0';
#endif
    return true;
}

bool build_bundle_path(char* output, size_t output_size, const char* relative_path)
{
    if (output == nullptr || output_size == 0 || !safe_bundle_relative_path(relative_path))
    {
        return false;
    }

    char base_path[4096] = {};
    if (!executable_directory(base_path, sizeof(base_path)))
    {
        return false;
    }

    const int written = std::snprintf(output, output_size, "%s%s", base_path, relative_path);
    return written > 0 && static_cast<size_t>(written) < output_size;
}

} // namespace

bool asset_path_build(char* output, size_t output_size, const char* relative_path)
{
    if (output == nullptr || output_size == 0 || relative_path == nullptr)
    {
        return false;
    }

    char asset_relative_path[4096] = {};
    const int written = std::snprintf(asset_relative_path, sizeof(asset_relative_path), "%s%s", AssetRoot, relative_path);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(asset_relative_path))
    {
        return false;
    }

    return build_bundle_path(output, output_size, asset_relative_path);
}

bool bundle_path_build(char* output, size_t output_size, const char* relative_path)
{
    if (output == nullptr || output_size == 0 || relative_path == nullptr)
    {
        return false;
    }

    return build_bundle_path(output, output_size, relative_path);
}
