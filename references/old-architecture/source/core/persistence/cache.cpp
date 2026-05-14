#include "core/persistence/internal.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace persistence_json {

namespace {

struct chunk_file_candidates {
    std::filesystem::path json_path{};
    std::filesystem::path zstd_path{};
};

auto pack_chunk_coord(int cx, int cz) -> std::uint64_t
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32u) |
           static_cast<std::uint32_t>(cz);
}

auto parse_chunk_file_name(std::string_view filename, int& cx, int& cz, bool& compressed) -> bool
{
    if (!filename.starts_with("chunk_"))
    {
        return false;
    }

    std::string_view token = filename.substr(6);
    compressed = false;
    if (token.ends_with(".json.zst"))
    {
        token.remove_suffix(9);
        compressed = true;
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

    const std::string cx_token(token.substr(0, separator));
    const std::string cz_token(token.substr(separator + 1));
    size_t parsed = 0;
    try
    {
        cx = std::stoi(cx_token, &parsed);
        if (parsed != cx_token.size())
        {
            return false;
        }
        cz = std::stoi(cz_token, &parsed);
        return parsed == cz_token.size();
    }
    catch (...)
    {
        return false;
    }
}

void remove_if_present(const std::filesystem::path& path, const char* description)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error) || error)
    {
        return;
    }
    std::filesystem::remove(path, error);
    if (error)
    {
        oct_log_errorf("Failed to remove %s: %s", description, path.string().c_str());
    }
}

} // namespace

void load_chunk_override_into_cache(chunk_cache_entry& chunk, const chunk_override_file& file, bool dirty)
{
    chunk.cx = file.cx;
    chunk.cz = file.cz;
    chunk.blocks.clear();
    for (const block_override_entry& block : file.blocks)
    {
        chunk.blocks[{block.bx, block.by, block.bz}] = static_cast<block_t>(block.block);
    }
    chunk.dirty = dirty;
}

void load_all_world_chunks(persistence_state& state)
{
    OCT_PROFILE_ZONE("persistence_load_all_world_chunks");
    const Uint64 start_ticks = oct_profile_now_ticks();
    int loaded_files = 0;
    const std::filesystem::path root = world_root_path(state);
    if (!std::filesystem::exists(root))
    {
        oct_log_infof("Startup timing | persistence_load_all_world_chunks loaded %d overrides in %.2f ms", loaded_files,
                      oct_profile_elapsed_ms(start_ticks));
        return;
    }

    std::unordered_map<std::uint64_t, chunk_file_candidates> candidates{};
    for (const auto& entry : std::filesystem::directory_iterator(root))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        int cx = 0;
        int cz = 0;
        bool compressed = false;
        const std::string filename = entry.path().filename().string();
        if (!parse_chunk_file_name(filename, cx, cz, compressed))
        {
            continue;
        }

        chunk_file_candidates& candidate = candidates[pack_chunk_coord(cx, cz)];
        if (compressed)
        {
            candidate.zstd_path = entry.path();
        }
        else
        {
            candidate.json_path = entry.path();
        }
    }

    for (const auto& [packed_coord, candidate] : candidates)
    {
        (void) packed_coord;

        const bool has_zstd = !candidate.zstd_path.empty();
        const bool has_json = !candidate.json_path.empty();
        const std::filesystem::path preferred_path = has_zstd ? candidate.zstd_path : candidate.json_path;
        if (preferred_path.empty())
        {
            continue;
        }

        if (has_zstd && has_json)
        {
            oct_log_infof("Ignoring legacy JSON chunk override in favor of canonical zstd file: %s",
                          candidate.json_path.string().c_str());
        }

        chunk_override_file file{};
        bool loaded_from_json = false;
        if (!read_chunk_override_file(preferred_path, file))
        {
            if (!(has_zstd && has_json && read_chunk_override_file(candidate.json_path, file)))
            {
                continue;
            }
            loaded_from_json = true;
            oct_log_infof("Recovered chunk override from legacy JSON after zstd read failure: %s",
                          candidate.json_path.string().c_str());
        }

        bool upgraded = false;
        if (!upgrade_chunk_override_file(file, &upgraded))
        {
            continue;
        }

        loaded_files += 1;
        chunk_cache_entry& chunk = state.chunks[{file.cx, file.cz}];
        load_chunk_override_into_cache(chunk, file, false);

        const bool needs_canonical_rewrite = loaded_from_json || !has_zstd || upgraded;
        if (needs_canonical_rewrite)
        {
            const std::filesystem::path canonical_path = chunk_zstd_path(state, file.cx, file.cz);
            if (write_chunk_override_file_atomic(canonical_path, file))
            {
                remove_if_present(candidate.json_path, "legacy JSON chunk override");
            }
        }
        else if (has_json)
        {
            remove_if_present(candidate.json_path, "stale duplicate JSON chunk override");
        }
    }

    oct_log_infof("Startup timing | persistence_load_all_world_chunks loaded %d overrides in %.2f ms", loaded_files,
                  oct_profile_elapsed_ms(start_ticks));
}

void flush_settings(persistence_state& state)
{
    if (!state.settings_dirty)
    {
        return;
    }
    if (write_json_file_atomic(settings_path(state), state.settings))
    {
        state.settings_dirty = false;
    }
}

void flush_lighting_tuning(persistence_state& state)
{
    if (!state.lighting_tuning_dirty)
    {
        return;
    }
    if (write_json_file_atomic(lighting_tuning_path(state), state.lighting_tuning))
    {
        state.lighting_tuning_dirty = false;
    }
}

void flush_world_time(persistence_state& state)
{
    if (!state.world_time_dirty)
    {
        return;
    }
    if (write_json_file_atomic(world_time_path(state), state.world_time))
    {
        state.world_time_dirty = false;
    }
}

void flush_players(persistence_state& state)
{
    for (auto& [id, player] : state.players)
    {
        if (!player.dirty)
        {
            continue;
        }
        if (write_json_file_atomic(player_path(state, id), player.data))
        {
            player.dirty = false;
        }
    }
}

void flush_chunks(persistence_state& state)
{
    OCT_PROFILE_ZONE("persistence_flush_chunks");
    for (auto& [key, chunk] : state.chunks)
    {
        if (!chunk.dirty)
        {
            continue;
        }
        if (write_chunk_override_file_atomic(chunk_zstd_path(state, key.cx, key.cz), make_chunk_override_file(chunk)))
        {
            remove_if_present(chunk_path(state, key.cx, key.cz), "legacy JSON chunk override");
            chunk.dirty = false;
        }
    }
}

} // namespace persistence_json
