#include "core/persistence/internal.h"

using namespace persistence_json;

void persistence_set_lighting_tuning(const void* data, int size)
{
    if (size != (int) sizeof(lighting_tuning_blob) || !data)
    {
        oct_log_errorf("Failed to set lighting tuning: incompatible blob size");
        return;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    const lighting_tuning_blob* blob = static_cast<const lighting_tuning_blob*>(data);
    g_persistence_state.lighting_tuning = to_lighting_tuning_file(*blob);
    g_persistence_state.lighting_tuning_loaded = true;
    g_persistence_state.lighting_tuning_dirty = true;
}

bool persistence_get_lighting_tuning(void* data, int size)
{
    if (size != (int) sizeof(lighting_tuning_blob) || !data)
    {
        oct_log_errorf("Failed to get lighting tuning: incompatible blob size");
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    if (!g_persistence_state.lighting_tuning_loaded)
    {
        g_persistence_state.lighting_tuning_loaded = true;
        const std::filesystem::path path = lighting_tuning_path(g_persistence_state);
        if (!std::filesystem::exists(path) || !read_json_file(path, g_persistence_state.lighting_tuning))
        {
            return false;
        }
    }
    if (g_persistence_state.lighting_tuning.version != 1)
    {
        return false;
    }
    const lighting_tuning_blob blob = to_lighting_tuning_blob(g_persistence_state.lighting_tuning);
    SDL_memcpy(data, &blob, sizeof(blob));
    return true;
}
