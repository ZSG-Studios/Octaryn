#include "core/persistence/internal.h"

using namespace persistence_json;

void persistence_set_player(int id, const void* data, int size)
{
    if (size != (int) sizeof(player_blob) || !data)
    {
        oct_log_errorf("Failed to set player: incompatible blob size");
        return;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    const player_blob* blob = static_cast<const player_blob*>(data);
    player_cache_entry& entry = g_persistence_state.players[id];
    entry.data = to_player_file(*blob);
    entry.loaded = true;
    entry.dirty = true;
}

bool persistence_get_player(int id, void* data, int size)
{
    if (size != (int) sizeof(player_blob) || !data)
    {
        oct_log_errorf("Failed to get player: incompatible blob size");
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    player_cache_entry& entry = g_persistence_state.players[id];
    if (!entry.loaded)
    {
        entry.loaded = true;
        const std::filesystem::path path = player_path(g_persistence_state, id);
        if (!std::filesystem::exists(path) || !read_json_file(path, entry.data))
        {
            return false;
        }
    }
    if (entry.data.version != 1)
    {
        return false;
    }
    const player_blob blob = to_player_blob(entry.data);
    SDL_memcpy(data, &blob, sizeof(blob));
    return true;
}
