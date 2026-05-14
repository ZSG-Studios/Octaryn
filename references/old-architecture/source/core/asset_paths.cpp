#include "core/asset_paths.h"

#include <string_view>

namespace {

constexpr std::string_view kAssetRoot{"assets/"};

} // namespace

bool asset_path_build(char* output, size_t output_size, const char* relative_path)
{
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr || relative_path == nullptr || output == nullptr || output_size == 0)
    {
        return false;
    }

    const int written = SDL_snprintf(
        output,
        output_size,
        "%s%.*s%s",
        base_path,
        static_cast<int>(kAssetRoot.size()),
        kAssetRoot.data(),
        relative_path);
    return written > 0 && static_cast<size_t>(written) < output_size;
}
