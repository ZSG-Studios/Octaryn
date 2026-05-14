#include "core/persistence/internal.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

#include "world/block/block.h"

using namespace persistence_json;

namespace persistence_json {

namespace {

constexpr Uint32 kWorldSaveMetadataVersion = 1;

auto parse_int_token(std::string_view token, int& value) -> bool
{
    if (token.empty())
    {
        return false;
    }

    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    return error == std::errc{} && ptr == end;
}

auto parse_player_file_id(std::string_view filename, int& id) -> bool
{
    if (!filename.starts_with("player_") || !filename.ends_with(".json"))
    {
        return false;
    }

    const std::string_view token = filename.substr(7, filename.size() - 12);
    return parse_int_token(token, id);
}

auto parse_chunk_file_key(std::string_view filename, int& cx, int& cz) -> bool
{
    if (!filename.starts_with("chunk_"))
    {
        return false;
    }

    std::string_view token = filename.substr(6);
    if (token.ends_with(".json.zst"))
    {
        token.remove_suffix(9);
    }
    else if (token.ends_with(".json"))
    {
        token.remove_suffix(5);
    }
    else
    {
        return false;
    }

    const size_t separator = token.find('_');
    if (separator == std::string_view::npos)
    {
        return false;
    }

    return parse_int_token(token.substr(0, separator), cx) && parse_int_token(token.substr(separator + 1), cz);
}

auto pack_chunk_coord(int cx, int cz) -> std::uint64_t
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32u) |
           static_cast<std::uint32_t>(cz);
}

auto count_saved_players(const std::filesystem::path& root) -> Uint32
{
    std::error_code error;
    if (!std::filesystem::exists(root, error) || error)
    {
        return 0;
    }

    std::filesystem::directory_iterator iterator(root, error);
    if (error)
    {
        oct_log_errorf("Failed to inspect player save files in: %s", root.string().c_str());
        return 0;
    }

    std::unordered_set<int> player_ids{};
    for (const auto& entry : iterator)
    {
        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error) || entry_error)
        {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        int player_id = 0;
        if (parse_player_file_id(filename, player_id))
        {
            player_ids.insert(player_id);
        }
    }

    return static_cast<Uint32>(player_ids.size());
}

auto count_saved_chunks(const std::filesystem::path& root) -> Uint32
{
    std::error_code error;
    if (!std::filesystem::exists(root, error) || error)
    {
        return 0;
    }

    std::filesystem::directory_iterator iterator(root, error);
    if (error)
    {
        oct_log_errorf("Failed to inspect chunk override files in: %s", root.string().c_str());
        return 0;
    }

    std::unordered_set<std::uint64_t> chunk_coords{};
    for (const auto& entry : iterator)
    {
        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error) || entry_error)
        {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        int cx = 0;
        int cz = 0;
        if (parse_chunk_file_key(filename, cx, cz))
        {
            chunk_coords.insert(pack_chunk_coord(cx, cz));
        }
    }

    return static_cast<Uint32>(chunk_coords.size());
}

} // namespace

auto build_world_save_metadata(const persistence_state& state) -> world_save_metadata_file
{
    world_save_metadata_file metadata{};
    metadata.version = kWorldSaveMetadataVersion;

    std::error_code error;
    metadata.has_settings = std::filesystem::exists(settings_path(state), error) && !error;
    error.clear();
    metadata.has_lighting_tuning = std::filesystem::exists(lighting_tuning_path(state), error) && !error;
    error.clear();
    metadata.has_world_time = std::filesystem::exists(world_time_path(state), error) && !error;
    metadata.player_count = count_saved_players(state.root);
    metadata.has_player_data = metadata.player_count > 0;
    metadata.chunk_override_count = count_saved_chunks(world_root_path(state));
    metadata.has_world_data = metadata.chunk_override_count > 0;
    // "World exists" is explicit save metadata, not just edited chunks.
    // Settings, lighting, world time, player blobs, and chunk data all count.
    metadata.save_exists = metadata.has_settings || metadata.has_lighting_tuning || metadata.has_world_time ||
                           metadata.has_player_data || metadata.has_world_data;
    return metadata;
}

bool persistence_world_save_data_exists(const persistence_state& state)
{
    return build_world_save_metadata(state).save_exists;
}

bool persistence_ensure_world_save_metadata(const persistence_state& state)
{
    const world_save_metadata_file metadata = build_world_save_metadata(state);
    const std::filesystem::path path = world_save_metadata_path(state);

    if (!metadata.save_exists)
    {
        std::error_code error;
        if (!std::filesystem::exists(path, error))
        {
            return !error;
        }

        std::filesystem::remove(path, error);
        if (error)
        {
            oct_log_errorf("Failed to remove stale world metadata file: %s", path.string().c_str());
            return false;
        }
        return true;
    }

    return write_json_file_atomic(path, metadata);
}

bool persistence_has_world_save(void)
{
    std::scoped_lock lock(g_persistence_state.mutex);
    return g_persistence_state.initialized && persistence_world_save_data_exists(g_persistence_state);
}

bool persistence_get_world_save_metadata(persistence_world_save_metadata_t* metadata)
{
    if (!metadata)
    {
        return false;
    }

    *metadata = {};

    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }

    const world_save_metadata_file file = build_world_save_metadata(g_persistence_state);
    metadata->version = file.version;
    metadata->save_exists = file.save_exists;
    metadata->has_settings = file.has_settings;
    metadata->has_lighting_tuning = file.has_lighting_tuning;
    metadata->has_world_time = file.has_world_time;
    metadata->has_player_data = file.has_player_data;
    metadata->has_world_data = file.has_world_data;
    metadata->player_count = file.player_count;
    metadata->chunk_override_count = file.chunk_override_count;
    return true;
}

bool persistence_has_world_overrides(void)
{
    std::scoped_lock lock(g_persistence_state.mutex);
    return g_persistence_state.initialized && !g_persistence_state.chunks.empty();
}

} // namespace persistence_json

bool persistence_has_world_save(void)
{
    return persistence_json::persistence_has_world_save();
}

bool persistence_get_world_save_metadata(persistence_world_save_metadata_t* metadata)
{
    return persistence_json::persistence_get_world_save_metadata(metadata);
}

bool persistence_has_world_overrides(void)
{
    return persistence_json::persistence_has_world_overrides();
}

void persistence_set_block(int cx, int cz, int bx, int by, int bz, block_t block)
{
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    if (block == BLOCK_CLOUD)
    {
        block = BLOCK_EMPTY;
    }
    chunk_cache_entry& chunk = g_persistence_state.chunks[{cx, cz}];
    chunk.cx = cx;
    chunk.cz = cz;
    chunk.blocks[{bx, by, bz}] = block;
    chunk.dirty = true;
}

void persistence_get_blocks(void* userdata, int cx, int cz, persistence_set_block_t function)
{
    if (!function)
    {
        return;
    }
    std::vector<std::pair<block_key, block_t>> snapshot{};
    {
        std::scoped_lock lock(g_persistence_state.mutex);
        if (!g_persistence_state.initialized)
        {
            return;
        }
        const auto it = g_persistence_state.chunks.find({cx, cz});
        if (it == g_persistence_state.chunks.end())
        {
            return;
        }
        snapshot.reserve(it->second.blocks.size());
        for (const auto& entry : it->second.blocks)
        {
            snapshot.push_back(entry);
        }
    }
    std::sort(snapshot.begin(), snapshot.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.first.by != rhs.first.by)
        {
            return lhs.first.by < rhs.first.by;
        }
        if (lhs.first.bx != rhs.first.bx)
        {
            return lhs.first.bx < rhs.first.bx;
        }
        return lhs.first.bz < rhs.first.bz;
    });
    for (const auto& [key, block] : snapshot)
    {
        if (block == BLOCK_CLOUD)
        {
            continue;
        }
        function(userdata, key.bx, key.by, key.bz, block);
    }
}
