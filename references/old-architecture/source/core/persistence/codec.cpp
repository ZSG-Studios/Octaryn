#include <lz4.h>
#include <zstd.h>

#include "core/persistence/internal.h"

namespace persistence_json {

bool compress_zstd(std::string_view input, std::vector<char>& output)
{
    output.resize(ZSTD_compressBound(input.size()));
    const size_t compressed_size = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(compressed_size))
    {
        oct_log_errorf("Failed to compress zstd payload: %s", ZSTD_getErrorName(compressed_size));
        return false;
    }
    output.resize(compressed_size);
    return true;
}

bool decompress_zstd(std::span<const char> input, std::string& output)
{
    const unsigned long long expected_size = ZSTD_getFrameContentSize(input.data(), input.size());
    if (expected_size == ZSTD_CONTENTSIZE_ERROR || expected_size == ZSTD_CONTENTSIZE_UNKNOWN)
    {
        oct_log_errorf("Failed to determine zstd frame content size");
        return false;
    }
    output.resize((size_t) expected_size);
    const size_t decompressed_size = ZSTD_decompress(output.data(), output.size(), input.data(), input.size());
    if (ZSTD_isError(decompressed_size))
    {
        oct_log_errorf("Failed to decompress zstd payload: %s", ZSTD_getErrorName(decompressed_size));
        return false;
    }
    if (decompressed_size != output.size())
    {
        oct_log_errorf("Unexpected decompressed zstd payload size");
        return false;
    }
    return true;
}

bool compress_lz4(std::string_view input, std::vector<char>& output)
{
    const int max_size = LZ4_compressBound((int) input.size());
    output.resize(sizeof(Uint32) + (size_t) max_size);
    const Uint32 original_size = (Uint32) input.size();
    SDL_memcpy(output.data(), &original_size, sizeof(original_size));
    const int compressed_size = LZ4_compress_default(input.data(), output.data() + sizeof(original_size), (int) input.size(), max_size);
    if (compressed_size <= 0)
    {
        oct_log_errorf("Failed to compress lz4 payload");
        return false;
    }
    output.resize(sizeof(original_size) + (size_t) compressed_size);
    return true;
}

bool decompress_lz4(const void* input, int size, std::string& output)
{
    if (!input || size < (int) sizeof(Uint32))
    {
        return false;
    }
    Uint32 original_size = 0;
    SDL_memcpy(&original_size, input, sizeof(original_size));
    output.resize(original_size);
    const int result =
      LZ4_decompress_safe((const char*) input + sizeof(original_size), output.data(), size - (int) sizeof(original_size), (int) original_size);
    if (result < 0 || (Uint32) result != original_size)
    {
        oct_log_errorf("Failed to decompress lz4 payload");
        return false;
    }
    return true;
}

} // namespace persistence_json
