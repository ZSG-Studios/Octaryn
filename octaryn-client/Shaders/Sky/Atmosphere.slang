#pragma once
// Original sky.frag.glsl atmosphere, square sun/moon helpers.
float get_square_mask(float2 uv, float half_extent, float softness)
{
    float square_distance = max(abs(uv.x), abs(uv.y)) - half_extent;
    float aa = max(softness, fwidth(square_distance));
    return 1.0 - smoothstep(-aa, aa, square_distance);
}

float get_smootherstep(float edge0, float edge1, float value)
{
    float t = saturate((value - edge0) / max(edge1 - edge0, 0.000001));
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float safe_pow01(float base, float exponent)
{
    return pow(saturate(base), exponent);
}

float3 get_safe_direction(float3 value, float3 fallback)
{
    float length_sq = dot(value, value);
    return length_sq > 0.00000001 ? value * rsqrt(length_sq) : fallback;
}

float3 sanitize_sky_color(float3 color, float3 fallback)
{
    bool3 invalid = bool3(isnan(color.x) || isinf(color.x),
                          isnan(color.y) || isinf(color.y),
                          isnan(color.z) || isinf(color.z));
    return float3(invalid.x ? fallback.x : color.x,
                invalid.y ? fallback.y : color.y,
                invalid.z ? fallback.z : color.z);
}

float get_square_halo(float2 uv, float body_half_extent, float halo_half_extent, float softness)
{
    float square_distance = max(abs(uv.x), abs(uv.y));
    float soft_body = body_half_extent + softness;
    float soft_halo = halo_half_extent + softness * 1.5;
    float falloff = 1.0 - get_smootherstep(soft_body, soft_halo, square_distance);
    return falloff * falloff * (3.0 - 2.0 * falloff);
}

float3 get_atmosphere_sky_color(float3 sky_direction,
                              float3 sun_direction,
                              float sky_visibility,
                              float twilight_strength,
                              float celestial_visibility)
{
    float above_horizon = saturate(sky_direction.y);
    float horizon = safe_pow01(1.0 - get_smootherstep(0.0, 1.0, abs(sky_direction.y)), 2.35);
    float upper_sky = get_smootherstep(0.0, 1.0, above_horizon);
    float zenith = safe_pow01(upper_sky, 0.70);
    float sun_height = sun_direction.y;
    float day = get_smootherstep(-0.08, 0.20, sun_height) * saturate(sky_visibility);
    float night = 1.0 - day;
    float twilight = max(saturate(twilight_strength),
                         get_smootherstep(-0.26, 0.07, sun_height) *
                         (1.0 - get_smootherstep(0.08, 0.36, sun_height)));
    float3 night_horizon = float3(0.016, 0.024, 0.056);
    float3 night_zenith = float3(0.012, 0.018, 0.048);
    float3 night_sky = lerp(night_horizon, night_zenith, upper_sky * 0.72);
    night_sky = max(night_sky, float3(0.011, 0.016, 0.042));
    night_sky += float3(0.006, 0.010, 0.024) * horizon * night * 0.28;

    float3 day_horizon = float3(0.62, 0.76, 0.94);
    float3 day_zenith = float3(0.055, 0.22, 0.56);
    float3 day_sky = lerp(day_horizon, day_zenith, zenith);
    day_sky += float3(1.0, 0.90, 0.72) * horizon * day * 0.10;

    float3 sunset = float3(1.0, 0.42, 0.13) * horizon * twilight * 0.56;
    float3 upper_twilight = float3(0.13, 0.12, 0.28) * zenith * twilight * (1.0 - day) * 0.28;

    float3 sky = lerp(night_sky, day_sky, day);
    sky += sunset + upper_twilight;
    return saturate(sanitize_sky_color(sky, night_zenith));
}

float3 get_flat_sky_color(float3 sun_direction,
                        float sky_visibility,
                        float twilight_strength,
                        float celestial_visibility)
{
    float3 horizon_color = get_atmosphere_sky_color(float3(0.0, 0.0, 1.0),
                                                  sun_direction,
                                                  sky_visibility,
                                                  twilight_strength,
                                                  celestial_visibility);
    float3 upper_color = get_atmosphere_sky_color(float3(0.0, 0.78, 0.62),
                                                sun_direction,
                                                sky_visibility,
                                                twilight_strength,
                                                celestial_visibility);
    return lerp(horizon_color, upper_color, 0.72);
}

float get_low_sun_factor(float sun_height)
{
    return (1.0 - smoothstep(0.08, 0.66, sun_height)) * smoothstep(-0.18, 0.16, sun_height);
}

float3 get_sun_core_color(float sun_height)
{
    float low_sun = get_low_sun_factor(sun_height);
    float horizon_red = 1.0 - smoothstep(-0.12, 0.16, sun_height);
    float3 noon = float3(1.0, 0.96, 0.80);
    float3 low = float3(1.0, 0.62, 0.24);
    float3 horizon = float3(1.0, 0.34, 0.10);
    return lerp(lerp(noon, low, low_sun), horizon, horizon_red);
}

float3 get_sun_halo_color(float sun_height)
{
    float low_sun = get_low_sun_factor(sun_height);
    float horizon_red = 1.0 - smoothstep(-0.14, 0.18, sun_height);
    float3 noon = float3(1.0, 0.64, 0.22);
    float3 low = float3(1.0, 0.34, 0.08);
    float3 horizon = float3(1.0, 0.22, 0.035);
    return lerp(lerp(noon, low, low_sun), horizon, horizon_red);
}
