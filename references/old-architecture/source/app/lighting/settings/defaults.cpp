#include "internal.h"

#include <cmath>

#include "core/log.h"

namespace {

constexpr float kDefaultFogDistance = 256.0f;
constexpr float kDefaultSkylightFloor = 0.08f;
constexpr float kDefaultAmbientStrength = 0.82f;
constexpr float kDefaultSunStrength = 1.0f;
constexpr float kDefaultSunFallbackStrength = 1.0f;

} // namespace

void main_lighting_settings_init(main_lighting_tuning_t* tuning)
{
    *tuning = {};
    tuning->fog_enabled = 1;
    tuning->fog_distance = kDefaultFogDistance;
    tuning->skylight_floor = kDefaultSkylightFloor;
    tuning->ambient_strength = kDefaultAmbientStrength;
    tuning->sun_strength = kDefaultSunStrength;
    tuning->sun_fallback_strength = kDefaultSunFallbackStrength;
}

void main_lighting_settings_sanitize(main_lighting_tuning_t* tuning)
{
    tuning->fog_distance = SDL_clamp(tuning->fog_distance, 64.0f, 2048.0f);
    tuning->skylight_floor = SDL_clamp(tuning->skylight_floor, 0.05f, 0.6f);
    tuning->ambient_strength = SDL_clamp(tuning->ambient_strength, 0.25f, 3.0f);
    tuning->sun_strength = SDL_clamp(tuning->sun_strength, 0.0f, 3.0f);
    tuning->sun_fallback_strength = SDL_clamp(tuning->sun_fallback_strength, 0.0f, 3.0f);
}
