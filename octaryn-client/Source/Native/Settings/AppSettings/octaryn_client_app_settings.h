#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OCTARYN_CLIENT_APP_SETTINGS_VERSION 8u
#define OCTARYN_CLIENT_APP_SETTINGS_DISPLAY_NAME_CAPACITY 128u

typedef struct octaryn_client_app_settings
{
    uint32_t version;
    uint8_t fog_enabled;
    uint8_t fullscreen;
    char display_name[OCTARYN_CLIENT_APP_SETTINGS_DISPLAY_NAME_CAPACITY];
    int32_t display_index;
    int32_t display_mode_width;
    int32_t display_mode_height;
    float display_mode_refresh_rate;
    uint8_t clouds_enabled;
    uint8_t sky_gradient_enabled;
    int32_t window_width;
    int32_t window_height;
    int32_t render_distance;
    uint8_t stars_enabled;
    uint8_t sun_enabled;
    uint8_t moon_enabled;
    uint8_t pom_enabled;
    uint8_t pbr_enabled;
    int32_t present_mode_index;
} octaryn_client_app_settings;

void octaryn_client_app_settings_default(octaryn_client_app_settings* settings);
int octaryn_client_app_settings_is_supported_version(uint32_t version);
int octaryn_client_app_settings_sanitize(octaryn_client_app_settings* settings);

#ifdef __cplusplus
}
#endif
