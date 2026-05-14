#include "core/persistence/internal.h"

using namespace persistence_json;

bool persistence_init(const char* path)
{
    OCT_PROFILE_ZONE("persistence_init");
    const Uint64 start_ticks = oct_profile_now_ticks();
    if (!path || !*path)
    {
        return false;
    }
    std::scoped_lock lock(g_persistence_state.mutex);
    reset_state(g_persistence_state);
    g_persistence_state.root = path;
    std::error_code error;
    std::filesystem::create_directories(g_persistence_state.root, error);
    std::filesystem::create_directories(world_root_path(g_persistence_state), error);
    if (error)
    {
        oct_log_errorf("Failed to create save root: %s", g_persistence_state.root.string().c_str());
        return false;
    }
    g_persistence_state.initialized = true;
    load_all_world_chunks(g_persistence_state);
    (void) persistence_ensure_world_save_metadata(g_persistence_state);
    oct_profile_log_duration("Startup timing", "persistence_init total", start_ticks);
    return true;
}

void persistence_free()
{
    persistence_commit();
    std::scoped_lock lock(g_persistence_state.mutex);
    reset_state(g_persistence_state);
}

void persistence_commit()
{
    OCT_PROFILE_ZONE("persistence_commit");
    std::scoped_lock lock(g_persistence_state.mutex);
    if (!g_persistence_state.initialized)
    {
        return;
    }
    flush_settings(g_persistence_state);
    flush_lighting_tuning(g_persistence_state);
    flush_world_time(g_persistence_state);
    flush_players(g_persistence_state);
    flush_chunks(g_persistence_state);
    (void) persistence_ensure_world_save_metadata(g_persistence_state);
}
