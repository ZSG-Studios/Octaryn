#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SETTINGS_VERSION 8u
#define APP_SETTINGS_DISPLAY_NAME_CAPACITY 128u

typedef struct app_settings
{
    uint32_t version;
    uint8_t fog_enabled;
    uint8_t fullscreen;
    char display_name[APP_SETTINGS_DISPLAY_NAME_CAPACITY];
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
} app_settings;

void app_settings_default(app_settings* settings);
int app_settings_is_supported_version(uint32_t version);
int app_settings_sanitize(app_settings* settings);

#ifdef __cplusplus
}
#endif
