#include "core/persistence/internal.h"

using namespace persistence_json;

void persistence_set_world_time(const void* data, int size)
{
    if (size != (int) sizeof(world_time_blob) || !data)
    {
        oct_log_errorf("Failed to set world time: incompatible blob size");
        return;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    const world_time_blob* blob = static_cast<const world_time_blob*>(data);
    g_persistence_state.world_time = to_world_time_file(*blob);
    g_persistence_state.world_time_loaded = true;
    g_persistence_state.world_time_dirty = true;
}

bool persistence_get_world_time(void* data, int size)
{
    if (size != (int) sizeof(world_time_blob) || !data)
    {
        oct_log_errorf("Failed to get world time: incompatible blob size");
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    if (!g_persistence_state.world_time_loaded)
    {
        g_persistence_state.world_time_loaded = true;
        const std::filesystem::path path = world_time_path(g_persistence_state);
        if (!std::filesystem::exists(path) || !read_json_file(path, g_persistence_state.world_time))
        {
            return false;
        }
    }
    if (g_persistence_state.world_time.version != 1)
    {
        return false;
    }
    const world_time_blob blob = to_world_time_blob(g_persistence_state.world_time);
    SDL_memcpy(data, &blob, sizeof(blob));
    return true;
}
