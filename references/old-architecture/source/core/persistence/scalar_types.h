#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "core/persistence/persistence.h"
#include "core/persistence/settings_limits.h"

namespace persistence_json {

struct app_settings_blob {
    Uint32 version;
    Uint8 fog_enabled;
    Uint8 fullscreen;
    char display_name[OCTARYN_SETTINGS_DISPLAY_NAME_CAPACITY];
    Sint32 display_index;
    Sint32 display_mode_width;
    Sint32 display_mode_height;
    float display_mode_refresh_rate;
    Uint8 clouds_enabled;
    Uint8 sky_gradient_enabled;
    Sint32 window_width;
    Sint32 window_height;
    Sint32 render_distance;
    Sint32 worldgen_threads;
    Uint8 stars_enabled;
    Uint8 sun_enabled;
    Uint8 moon_enabled;
    Uint8 pom_enabled;
    Uint8 pbr_enabled;
};

struct player_blob {
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    block_t block;
};

struct settings_file {
    Uint32 version = 7;
    bool fog_enabled = true;
    bool fullscreen = false;
    std::string display_name{};
    Sint32 display_index = 0;
    Sint32 display_mode_width = 0;
    Sint32 display_mode_height = 0;
    float display_mode_refresh_rate = 0.0f;
    bool clouds_enabled = true;
    bool sky_gradient_enabled = true;
    Sint32 window_width = 0;
    Sint32 window_height = 0;
    Sint32 render_distance = 16;
    Sint32 worldgen_threads = 0;
    bool stars_enabled = true;
    bool sun_enabled = true;
    bool moon_enabled = true;
    bool pom_enabled = true;
    bool pbr_enabled = true;
};

struct lighting_tuning_blob {
    Uint32 version;
    float ambient_strength;
    float sun_strength;
    float sun_fallback_strength;
    float fog_distance;
    float skylight_floor;
};

struct lighting_tuning_file {
    Uint32 version = 1;
    float ambient_strength = 0.82f;
    float sun_strength = 1.0f;
    float sun_fallback_strength = 1.0f;
    float fog_distance = 256.0f;
    float skylight_floor = 0.08f;
};

struct world_time_blob {
    Uint32 version;
    Uint64 day_index;
    double seconds_of_day;
};

struct world_time_file {
    Uint32 version = 1;
    Uint64 day_index = 0;
    double seconds_of_day = 0.0;
};

struct player_file {
    Uint32 version = 1;
    float x = -200.0f;
    float y = 50.0f;
    float z = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    Uint32 block = BLOCK_YELLOW_TORCH;
};

} // namespace persistence_json
