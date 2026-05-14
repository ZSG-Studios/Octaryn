#include "core/persistence/internal.h"

using namespace persistence_json;

bool persistence_export_chunk_snapshot_lz4(int cx, int cz, void** data, int* size)
{
    if (!data || !size)
    {
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    const auto it = g_persistence_state.chunks.find({cx, cz});
    if (it == g_persistence_state.chunks.end())
    {
        return false;
    }
    const chunk_override_file file = make_chunk_override_file(it->second);
    std::string payload{};
    const auto write_error = glz::write<k_json_write_opts>(file, payload);
    if (write_error)
    {
        oct_log_errorf("Failed to encode lz4 chunk snapshot");
        return false;
    }
    std::vector<char> compressed{};
    if (!compress_lz4(payload, compressed))
    {
        return false;
    }
    void* allocation = SDL_malloc(compressed.size());
    if (!allocation)
    {
        return false;
    }
    SDL_memcpy(allocation, compressed.data(), compressed.size());
    *data = allocation;
    *size = static_cast<int>(compressed.size());
    return true;
}

bool persistence_import_chunk_snapshot_lz4(const void* data, int size)
{
    std::string payload{};
    if (!decompress_lz4(data, size, payload))
    {
        return false;
    }
    chunk_override_file file{};
    const auto error = glz::read<k_json_read_opts>(file, std::string_view{payload.data(), payload.size()});
    if (error)
    {
        oct_log_errorf("Failed to parse lz4 chunk snapshot");
        return false;
    }
    if (!upgrade_chunk_override_file(file, nullptr))
    {
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    chunk_cache_entry& chunk = g_persistence_state.chunks[{file.cx, file.cz}];
    load_chunk_override_into_cache(chunk, file, true);
    return true;
}

void persistence_free_export_buffer(void* data)
{
    SDL_free(data);
}
