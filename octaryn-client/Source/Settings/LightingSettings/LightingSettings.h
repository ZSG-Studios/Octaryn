#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lighting_settings
{
    uint8_t fog_enabled;
    float fog_distance;
    float skylight_floor;
    float ambient_strength;
    float sun_strength;
    float sun_fallback_strength;
} lighting_settings;

void lighting_settings_default(lighting_settings* settings);
lighting_settings lighting_settings_default_value(void);
int lighting_settings_sanitize(lighting_settings* settings);

#ifdef __cplusplus
}
#endif
