#include "core/persistence/internal.h"

using namespace persistence_json;

bool persistence_export_save_gzip(const char* path)
{
    if (!path || !*path)
    {
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    save_export_bundle bundle{};
    bundle.settings = g_persistence_state.settings;
    bundle.world_time = g_persistence_state.world_time;
    for (const auto& [id, player] : g_persistence_state.players)
    {
        bundle.players.push_back({.id = id, .data = player.data});
    }
    for (const auto& [key, chunk] : g_persistence_state.chunks)
    {
        (void) key;
        bundle.chunks.push_back(make_chunk_override_file(chunk));
    }
    std::string payload{};
    const auto write_error = glz::write<k_json_write_opts>(bundle, payload);
    if (write_error)
    {
        oct_log_errorf("Failed to encode gzip save export bundle");
        return false;
    }
    return write_gzip_file(path, payload);
}

bool persistence_import_save_gzip(const char* path)
{
    if (!path || !*path)
    {
        return false;
    }
    std::string payload{};
    if (!read_gzip_file(path, payload))
    {
        return false;
    }
    save_export_bundle bundle{};
    const auto error = glz::read<k_json_read_opts>(bundle, std::string_view{payload.data(), payload.size()});
    if (error)
    {
        oct_log_errorf("Failed to parse gzip save import bundle");
        return false;
    }

    ankerl::unordered_dense::map<int, player_cache_entry> imported_players{};
    for (const player_export_entry& player : bundle.players)
    {
        imported_players[player.id] = {.data = player.data, .loaded = true, .dirty = true};
    }

    ankerl::unordered_dense::map<chunk_key, chunk_cache_entry, chunk_key_hash> imported_chunks{};
    for (chunk_override_file chunk_file : bundle.chunks)
    {
        if (!upgrade_chunk_override_file(chunk_file, nullptr))
        {
            return false;
        }
        chunk_cache_entry& chunk = imported_chunks[{chunk_file.cx, chunk_file.cz}];
        load_chunk_override_into_cache(chunk, chunk_file, true);
    }

    std::scoped_lock lock(g_persistence_state.mutex);
    g_persistence_state.settings = bundle.settings;
    g_persistence_state.settings_loaded = true;
    g_persistence_state.settings_dirty = true;
    g_persistence_state.world_time = bundle.world_time;
    g_persistence_state.world_time_loaded = true;
    g_persistence_state.world_time_dirty = true;
    g_persistence_state.players = std::move(imported_players);
    g_persistence_state.chunks = std::move(imported_chunks);
    return true;
}
