#include "LightingSettings.h"

namespace {

constexpr float kDefaultFogDistance = 256.0f;
constexpr float kDefaultSkylightFloor = 0.08f;
constexpr float kDefaultAmbientStrength = 0.82f;
constexpr float kDefaultSunStrength = 1.0f;
constexpr float kDefaultSunFallbackStrength = 1.0f;

auto clamp_float(float value, float minimum, float maximum) -> float
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

auto normalize_flag(uint8_t value) -> uint8_t
{
    return value != 0u ? 1u : 0u;
}

} // namespace

void lighting_settings_default(lighting_settings* settings)
{
    if (settings == nullptr)
    {
        return;
    }

    *settings = {};
    settings->fog_enabled = 1u;
    settings->fog_distance = kDefaultFogDistance;
    settings->skylight_floor = kDefaultSkylightFloor;
    settings->ambient_strength = kDefaultAmbientStrength;
    settings->sun_strength = kDefaultSunStrength;
    settings->sun_fallback_strength = kDefaultSunFallbackStrength;
}

lighting_settings lighting_settings_default_value(void)
{
    lighting_settings settings{};
    lighting_settings_default(&settings);
    return settings;
}

int lighting_settings_sanitize(lighting_settings* settings)
{
    if (settings == nullptr)
    {
        return 0;
    }

    settings->fog_enabled = normalize_flag(settings->fog_enabled);
    settings->fog_distance = clamp_float(settings->fog_distance, 64.0f, 2048.0f);
    settings->skylight_floor = clamp_float(settings->skylight_floor, 0.05f, 0.6f);
    settings->ambient_strength = clamp_float(settings->ambient_strength, 0.25f, 3.0f);
    settings->sun_strength = clamp_float(settings->sun_strength, 0.0f, 3.0f);
    settings->sun_fallback_strength = clamp_float(settings->sun_fallback_strength, 0.0f, 3.0f);
    return 1;
}
