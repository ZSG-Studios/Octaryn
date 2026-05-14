#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "core/persistence/scalar_types.h"

namespace persistence_json {

inline constexpr int kChunkWidth = 32;
inline constexpr Uint32 kCurrentChunkOverrideFileVersion = 2;

struct world_save_metadata_file {
    Uint32 version = 1;
    bool save_exists = false;
    bool has_settings = false;
    bool has_lighting_tuning = false;
    bool has_world_time = false;
    bool has_player_data = false;
    bool has_world_data = false;
    Uint32 player_count = 0;
    Uint32 chunk_override_count = 0;
};

struct block_override_entry {
    Sint32 bx = 0;
    Sint32 by = 0;
    Sint32 bz = 0;
    Uint32 block = BLOCK_EMPTY;
};

struct chunk_override_file {
    Uint32 version = kCurrentChunkOverrideFileVersion;
    Sint32 cx = 0;
    Sint32 cz = 0;
    std::vector<block_override_entry> blocks{};
};

struct player_export_entry {
    Sint32 id = 0;
    player_file data{};
};

struct save_export_bundle {
    Uint32 version = 1;
    settings_file settings{};
    world_time_file world_time{};
    std::vector<player_export_entry> players{};
    std::vector<chunk_override_file> chunks{};
};

} // namespace persistence_json
