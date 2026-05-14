#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <mutex>

#include <ankerl/unordered_dense.h>

#include "core/persistence/cache_types.h"
#include "core/persistence/scalar_types.h"

namespace persistence_json {

struct player_cache_entry {
    player_file data{};
    bool loaded = false;
    bool dirty = false;
};

struct chunk_cache_entry {
    int cx = 0;
    int cz = 0;
    bool dirty = false;
    ankerl::unordered_dense::map<block_key, block_t, block_key_hash> blocks{};
};

struct persistence_state {
    std::filesystem::path root{};
    bool initialized = false;
    std::mutex mutex{};

    settings_file settings{};
    bool settings_loaded = false;
    bool settings_dirty = false;

    lighting_tuning_file lighting_tuning{};
    bool lighting_tuning_loaded = false;
    bool lighting_tuning_dirty = false;

    world_time_file world_time{};
    bool world_time_loaded = false;
    bool world_time_dirty = false;

    ankerl::unordered_dense::map<int, player_cache_entry> players{};
    ankerl::unordered_dense::map<chunk_key, chunk_cache_entry, chunk_key_hash> chunks{};
};

extern persistence_state g_persistence_state;

} // namespace persistence_json
