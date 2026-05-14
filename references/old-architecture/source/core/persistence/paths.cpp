#include "core/persistence/internal.h"

namespace persistence_json {

persistence_state g_persistence_state{};

void reset_state(persistence_state& state)
{
    state.root.clear();
    state.initialized = false;
    state.settings = {};
    state.settings_loaded = false;
    state.settings_dirty = false;
    state.lighting_tuning = {};
    state.lighting_tuning_loaded = false;
    state.lighting_tuning_dirty = false;
    state.world_time = {};
    state.world_time_loaded = false;
    state.world_time_dirty = false;
    state.players.clear();
    state.chunks.clear();
}

auto settings_path(const persistence_state& state) -> std::filesystem::path
{
    return state.root / "settings.json";
}

auto lighting_tuning_path(const persistence_state& state) -> std::filesystem::path
{
    return state.root / "lighting_tuning.json";
}

auto world_time_path(const persistence_state& state) -> std::filesystem::path
{
    return state.root / "world_time.json";
}

auto world_save_metadata_path(const persistence_state& state) -> std::filesystem::path
{
    return state.root / "world_meta.json";
}

auto player_path(const persistence_state& state, int id) -> std::filesystem::path
{
    return state.root / ("player_" + std::to_string(id) + ".json");
}

auto world_root_path(const persistence_state& state) -> std::filesystem::path
{
    return state.root / "world";
}

auto chunk_path(const persistence_state& state, int cx, int cz) -> std::filesystem::path
{
    return world_root_path(state) / ("chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".json");
}

auto chunk_zstd_path(const persistence_state& state, int cx, int cz) -> std::filesystem::path
{
    return world_root_path(state) / ("chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".json.zst");
}

} // namespace persistence_json
