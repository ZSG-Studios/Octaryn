#pragma once
#include "LightingSettings.h"
namespace octaryn::client::rendering {
struct SkySettings { bool gradient{true},stars{true},sun{true},moon{true}; };
struct SkyUniforms {
    float light_direction_sky[4]{};
    float twilight_celestial_time[4]{};
    float celestial_toggles[4]{};
};
struct SkyLighting {
    float visual_sky_visibility{},gameplay_sky_visibility{},skylight_floor{};
    float ambient_strength{},sun_strength{},sun_fallback_strength{};
};
SkyUniforms make_sky_uniforms(double day_fraction,double animation_seconds,const SkySettings& settings);
SkyLighting make_sky_lighting(double day_fraction,const lighting_settings& settings);
}
