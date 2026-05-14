#pragma once

#include <SDL3/SDL.h>

#include "core/persistence/settings_limits.h"

typedef struct app_settings
{
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
}
app_settings_t;

bool app_settings_load(app_settings_t* out_settings);
void app_settings_save(const app_settings_t* settings);
