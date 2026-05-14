#include "core/persistence/internal.h"

namespace persistence_json {

auto make_chunk_override_file(const chunk_cache_entry& chunk) -> chunk_override_file
{
    chunk_override_file file{};
    file.cx = chunk.cx;
    file.cz = chunk.cz;
    file.blocks.reserve(chunk.blocks.size());
    for (const auto& [block_key_value, block] : chunk.blocks)
    {
        file.blocks.push_back({
            .bx = block_key_value.bx,
            .by = block_key_value.by,
            .bz = block_key_value.bz,
            .block = block,
        });
    }
    std::sort(file.blocks.begin(), file.blocks.end(), [](const block_override_entry& lhs, const block_override_entry& rhs) {
        if (lhs.by != rhs.by)
        {
            return lhs.by < rhs.by;
        }
        if (lhs.bx != rhs.bx)
        {
            return lhs.bx < rhs.bx;
        }
        return lhs.bz < rhs.bz;
    });
    return file;
}

bool upgrade_chunk_override_file(chunk_override_file& value, bool* changed)
{
    if (changed)
    {
        *changed = false;
    }

    if (value.version == kCurrentChunkOverrideFileVersion)
    {
        return true;
    }

    if (value.version != 1)
    {
        oct_log_errorf("Unsupported chunk override file version: %u", value.version);
        return false;
    }

    bool saw_local_only = false;
    bool saw_world_only = false;
    for (const block_override_entry& block : value.blocks)
    {
        const bool already_world_coords = block.bx >= value.cx - 1 && block.bx <= value.cx + kChunkWidth &&
                                          block.bz >= value.cz - 1 && block.bz <= value.cz + kChunkWidth;
        const bool looks_like_local_coords =
            block.bx >= -1 && block.bx <= kChunkWidth && block.bz >= -1 && block.bz <= kChunkWidth;

        if (!already_world_coords && !looks_like_local_coords)
        {
            oct_log_errorf("Chunk override file version 1 has invalid coordinates for chunk (%d, %d)", value.cx, value.cz);
            return false;
        }

        saw_local_only |= looks_like_local_coords && !already_world_coords;
        saw_world_only |= already_world_coords && !looks_like_local_coords;
    }

    if (saw_local_only && saw_world_only)
    {
        oct_log_errorf("Chunk override file version 1 mixes local and world coordinates for chunk (%d, %d)",
                       value.cx,
                       value.cz);
        return false;
    }

    if (!saw_local_only && !saw_world_only && !value.blocks.empty())
    {
        oct_log_errorf("Chunk override file version 1 is ambiguous for chunk (%d, %d); explicit migration required",
                       value.cx,
                       value.cz);
        return false;
    }

    if (saw_local_only)
    {
        for (block_override_entry& block : value.blocks)
        {
            block.bx += value.cx;
            block.bz += value.cz;
        }
    }

    value.version = kCurrentChunkOverrideFileVersion;
    if (changed)
    {
        *changed = true;
    }
    return true;
}

bool write_chunk_override_file_atomic(const std::filesystem::path& path, const chunk_override_file& value)
{
    std::string payload{};
    auto write_error = glz::write<k_json_write_opts>(value, payload);
    if (write_error)
    {
        oct_log_errorf("Failed to encode chunk override payload: %s", path.string().c_str());
        return false;
    }

    std::vector<char> compressed{};
    if (!compress_zstd(payload, compressed))
    {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        oct_log_errorf("Failed to create chunk save directory: %s", path.parent_path().string().c_str());
        return false;
    }

    const std::filesystem::path temp_path = path.string() + ".tmp";
    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            oct_log_errorf("Failed to open temp chunk save file: %s", temp_path.string().c_str());
            return false;
        }
        file.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
        if (!file.good())
        {
            oct_log_errorf("Failed to write temp chunk save file: %s", temp_path.string().c_str());
            return false;
        }
    }

    std::filesystem::rename(temp_path, path, error);
    if (error)
    {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temp_path, path, error);
    }
    if (error)
    {
        oct_log_errorf("Failed to replace chunk save file: %s", path.string().c_str());
        std::filesystem::remove(temp_path, error);
        return false;
    }
    return true;
}

bool read_chunk_override_file(const std::filesystem::path& path, chunk_override_file& value)
{
    if (path.extension() == ".json")
    {
        return read_json_file(path, value);
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    const std::vector<char> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string payload{};
    if (!decompress_zstd(compressed, payload))
    {
        return false;
    }
    const auto error = glz::read<k_json_read_opts>(value, std::string_view{payload.data(), payload.size()});
    if (error)
    {
        oct_log_errorf("Failed to parse compressed chunk override file: %s", path.string().c_str());
        return false;
    }
    return true;
}

} // namespace persistence_json
