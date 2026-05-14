#include "core/persistence/internal.h"

using namespace persistence_json;

void persistence_set_settings(const void* data, int size)
{
    if (size != (int) sizeof(app_settings_blob) || !data)
    {
        oct_log_errorf("Failed to set settings: incompatible blob size");
        return;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    const app_settings_blob* blob = static_cast<const app_settings_blob*>(data);
    g_persistence_state.settings = to_settings_file(*blob);
    g_persistence_state.settings_loaded = true;
    g_persistence_state.settings_dirty = true;
}

bool persistence_get_settings(void* data, int size)
{
    if (size != (int) sizeof(app_settings_blob) || !data)
    {
        oct_log_errorf("Failed to get settings: incompatible blob size (got %d, expected %d)", size, (int) sizeof(app_settings_blob));
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return false;
    }
    if (!g_persistence_state.settings_loaded)
    {
        g_persistence_state.settings_loaded = true;
        const std::filesystem::path path = settings_path(g_persistence_state);
        if (!std::filesystem::exists(path) || !read_json_file(path, g_persistence_state.settings))
        {
            return false;
        }
    }
    if (g_persistence_state.settings.version < 1 || g_persistence_state.settings.version > 7)
    {
        return false;
    }
    const app_settings_blob blob = to_settings_blob(g_persistence_state.settings);
    SDL_memcpy(data, &blob, sizeof(blob));
    return true;
}
