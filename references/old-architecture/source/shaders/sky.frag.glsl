#version 450

layout(set = 3, binding = 1) uniform ProjUniforms { mat4 Proj; };
layout(set = 3, binding = 2) uniform ViewUniforms { mat4 View; };

#include "shader_common.glsl"

layout(set = 3, binding = 0) uniform SkyUniforms {
    vec4 LightDirectionAndSkyVisibility;
    vec4 TwilightCelestialCloudTime;
    vec4 CameraPositionAndCloudHeight;
    vec4 CelestialToggles;
};

layout(location = 0) in vec3 vLocalPosition;

layout(location = 0) out vec4 outColor;

float get_square_mask(vec2 uv, float half_extent, float softness)
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

vec3 get_safe_direction(vec3 value, vec3 fallback)
{
    float length_sq = dot(value, value);
    return length_sq > 0.00000001 ? value * inversesqrt(length_sq) : fallback;
}

vec3 sanitize_sky_color(vec3 color, vec3 fallback)
{
    bvec3 invalid = bvec3(isnan(color.x) || isinf(color.x),
                          isnan(color.y) || isinf(color.y),
                          isnan(color.z) || isinf(color.z));
    return vec3(invalid.x ? fallback.x : color.x,
                invalid.y ? fallback.y : color.y,
                invalid.z ? fallback.z : color.z);
}

float get_square_halo(vec2 uv, float body_half_extent, float halo_half_extent, float softness)
{
    float square_distance = max(abs(uv.x), abs(uv.y));
    float soft_body = body_half_extent + softness;
    float soft_halo = halo_half_extent + softness * 1.5;
    float falloff = 1.0 - get_smootherstep(soft_body, soft_halo, square_distance);
    return falloff * falloff * (3.0 - 2.0 * falloff);
}

vec3 get_atmosphere_sky_color(vec3 sky_direction,
                              vec3 sun_direction,
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
    vec3 night_horizon = vec3(0.016, 0.024, 0.056);
    vec3 night_zenith = vec3(0.012, 0.018, 0.048);
    vec3 night_sky = mix(night_horizon, night_zenith, upper_sky * 0.72);
    night_sky = max(night_sky, vec3(0.011, 0.016, 0.042));
    night_sky += vec3(0.006, 0.010, 0.024) * horizon * night * 0.28;

    vec3 day_horizon = vec3(0.62, 0.76, 0.94);
    vec3 day_zenith = vec3(0.055, 0.22, 0.56);
    vec3 day_sky = mix(day_horizon, day_zenith, zenith);
    day_sky += vec3(1.0, 0.90, 0.72) * horizon * day * 0.10;

    vec3 sunset = vec3(1.0, 0.42, 0.13) * horizon * twilight * 0.56;
    vec3 upper_twilight = vec3(0.13, 0.12, 0.28) * zenith * twilight * (1.0 - day) * 0.28;

    vec3 sky = mix(night_sky, day_sky, day);
    sky += sunset + upper_twilight;
    return saturate(sanitize_sky_color(sky, night_zenith));
}

vec3 get_flat_sky_color(vec3 sun_direction,
                        float sky_visibility,
                        float twilight_strength,
                        float celestial_visibility)
{
    vec3 horizon_color = get_atmosphere_sky_color(vec3(0.0, 0.0, 1.0),
                                                  sun_direction,
                                                  sky_visibility,
                                                  twilight_strength,
                                                  celestial_visibility);
    vec3 upper_color = get_atmosphere_sky_color(vec3(0.0, 0.78, 0.62),
                                                sun_direction,
                                                sky_visibility,
                                                twilight_strength,
                                                celestial_visibility);
    return mix(horizon_color, upper_color, 0.72);
}

float get_low_sun_factor(float sun_height)
{
    return (1.0 - smoothstep(0.08, 0.66, sun_height)) * smoothstep(-0.18, 0.16, sun_height);
}

vec3 get_sun_core_color(float sun_height)
{
    float low_sun = get_low_sun_factor(sun_height);
    float horizon_red = 1.0 - smoothstep(-0.12, 0.16, sun_height);
    vec3 noon = vec3(1.0, 0.96, 0.80);
    vec3 low = vec3(1.0, 0.62, 0.24);
    vec3 horizon = vec3(1.0, 0.34, 0.10);
    return mix(mix(noon, low, low_sun), horizon, horizon_red);
}

vec3 get_sun_halo_color(float sun_height)
{
    float low_sun = get_low_sun_factor(sun_height);
    float horizon_red = 1.0 - smoothstep(-0.14, 0.18, sun_height);
    vec3 noon = vec3(1.0, 0.64, 0.22);
    vec3 low = vec3(1.0, 0.34, 0.08);
    vec3 horizon = vec3(1.0, 0.22, 0.035);
    return mix(mix(noon, low, low_sun), horizon, horizon_red);
}

float get_star_hash(vec3 value)
{
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

vec3 get_star_random(vec3 value)
{
    return vec3(get_star_hash(value + vec3(11.0, 31.0, 57.0)),
                get_star_hash(value + vec3(71.0, 19.0, 43.0)),
                get_star_hash(value + vec3(37.0, 83.0, 17.0)));
}

vec2 get_star_projection(vec3 direction)
{
    float hemisphere_scale = max(direction.y + 1.18, 0.24);
    return direction.xz / hemisphere_scale;
}

float get_star_fade(vec3 cell, float time_seconds)
{
    float phase = get_star_hash(cell + vec3(101.0, 7.0, 61.0));
    float speed = mix(0.055, 0.185, get_star_hash(cell + vec3(3.0, 97.0, 41.0)));
    float cycle = fract(time_seconds * speed + phase);
    float pulse = 1.0 - abs(cycle * 2.0 - 1.0);
    pulse = pulse * pulse * (3.0 - 2.0 * pulse);
    return mix(0.18, 1.0, pulse);
}

vec3 get_star_layer(vec3 sky_direction, float cells, float density_threshold, float base_size, float layer_seed, float time_seconds)
{
    vec2 projected = get_star_projection(sky_direction);
    vec2 grid_position = projected * cells + vec2(layer_seed * 0.131, layer_seed * 0.071);
    vec2 cell = floor(grid_position);
    vec2 cell_uv = fract(grid_position);
    vec3 hash_cell = vec3(cell, layer_seed);
    float roll = get_star_hash(hash_cell + vec3(5.0, 13.0, 29.0));
    if (roll < density_threshold)
    {
        return vec3(0.0);
    }

    vec3 random = get_star_random(hash_cell);
    vec2 star_position = mix(vec2(0.22), vec2(0.78), random.xy);
    vec2 offset = cell_uv - star_position;
    float size = base_size * mix(0.70, 1.20, get_star_hash(hash_cell + vec3(23.0, 47.0, 89.0)));
    float square_distance = max(abs(offset.x), abs(offset.y));
    float aa = min(fwidth(square_distance) * 0.75, size * 0.28);
    float core = 1.0 - smoothstep(size - aa, size + aa, square_distance);
    float fade = get_star_fade(hash_cell, time_seconds);
    float brightness = mix(0.62, 1.25, get_star_hash(hash_cell + vec3(59.0, 2.0, 109.0)));
    vec3 tint = mix(vec3(0.72, 0.82, 1.0), vec3(1.0, 0.94, 0.76), get_star_hash(hash_cell + vec3(17.0, 67.0, 11.0)));
    return tint * core * brightness * fade;
}

vec3 get_stars(vec3 sky_direction, vec3 moon_direction, float sun_height, float celestial_visibility, float time_seconds)
{
    float horizon_mask = get_smootherstep(-0.04, 0.16, sky_direction.y);
    float night_mask = 1.0 - get_smootherstep(-0.16, 0.08, sun_height);
    float moon_glare = 1.0 - 0.55 * smoothstep(0.84, 0.995, dot(sky_direction, moon_direction));
    float visibility = saturate((1.0 - celestial_visibility) * horizon_mask * night_mask * moon_glare);
    if (visibility <= 0.001)
    {
        return vec3(0.0);
    }

    vec3 primary = get_star_layer(sky_direction, 82.0, 0.972, 0.048, 17.0, time_seconds);
    vec3 secondary = get_star_layer(sky_direction, 137.0, 0.986, 0.038, 53.0, time_seconds + 19.7);
    return (primary + secondary * 0.74) * visibility * 1.90;
}

void main()
{
    gl_FragDepth = 1.0;
    vec3 sky_direction = get_safe_direction(vLocalPosition, vec3(0.0, 1.0, 0.0));
    vec3 light_direction = LightDirectionAndSkyVisibility.xyz;
    float sky_visibility = LightDirectionAndSkyVisibility.w;
    float twilight_strength = TwilightCelestialCloudTime.x;
    float celestial_visibility = TwilightCelestialCloudTime.y;
    float sky_gradient_enabled = TwilightCelestialCloudTime.z;
    float star_time_seconds = TwilightCelestialCloudTime.w;
    float stars_enabled = step(0.5, CelestialToggles.x);
    float sun_enabled = step(0.5, CelestialToggles.y);
    float moon_enabled = step(0.5, CelestialToggles.z);

    vec3 sun_direction = get_safe_direction(-light_direction, vec3(0.0, -1.0, 0.0));
    vec3 flat_color = vec3(0.0);
    vec3 color = vec3(0.0);
    if (sky_gradient_enabled >= 0.5)
    {
        color = get_atmosphere_sky_color(sky_direction,
                                         sun_direction,
                                         sky_visibility,
                                         twilight_strength,
                                         celestial_visibility);
        flat_color = color;
    }
    else
    {
        flat_color = get_flat_sky_color(sun_direction,
                                        sky_visibility,
                                        twilight_strength,
                                        celestial_visibility);
        color = flat_color;
    }
    vec3 moon_direction = -sun_direction;
    if (stars_enabled > 0.5)
    {
        color += get_stars(sky_direction, moon_direction, sun_direction.y, celestial_visibility, star_time_seconds);
    }

    vec3 helper = abs(sun_direction.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(helper, sun_direction));
    vec3 up = normalize(cross(sun_direction, right));
    vec2 sun_uv = vec2(dot(sky_direction, right), dot(sky_direction, up));
    float sun_facing = dot(sky_direction, sun_direction);
    float sun_visible_mask = get_smootherstep(-0.18, -0.06, sun_direction.y);
    float sun_facing_mask = smoothstep(0.0, 0.02, sun_facing) * sun_visible_mask * sun_enabled;
    float sun_square = get_square_mask(sun_uv, 0.056, 0.0022) * sun_facing_mask;
    float sun_glow = get_square_halo(sun_uv, 0.056, 0.084, 0.009) * sun_facing_mask;
    float sun_outer_glow = get_square_halo(sun_uv, 0.084, 0.118, 0.016) * sun_facing_mask;
    vec3 sun_core_color = get_sun_core_color(sun_direction.y);
    vec3 sun_halo_color = get_sun_halo_color(sun_direction.y);
    vec3 sun_inner_halo_color = mix(sun_halo_color, vec3(1.0, 0.62, 0.16), 0.42);
    vec3 sun_outer_halo_color = mix(sun_halo_color, vec3(1.0, 0.24, 0.035), 0.58);
    color += sun_outer_halo_color * 0.34 * sun_outer_glow +
             sun_inner_halo_color * 0.48 * sun_glow;
    color = mix(color, sun_core_color * 1.45, saturate(sun_square * 0.95));
    color += sun_core_color * 0.55 * sun_square;

    vec3 moon_helper = abs(moon_direction.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 moon_right = normalize(cross(moon_helper, moon_direction));
    vec3 moon_up = normalize(cross(moon_direction, moon_right));
    vec2 moon_uv = vec2(dot(sky_direction, moon_right), dot(sky_direction, moon_up));
    float moon_facing = dot(sky_direction, moon_direction);
    float moon_visible_mask = get_smootherstep(-0.18, -0.06, moon_direction.y);
    float moon_facing_mask = smoothstep(0.0, 0.02, moon_facing) * moon_visible_mask * moon_enabled;
    float moon_disc = get_square_mask(moon_uv, 0.015, 0.0012) * moon_facing_mask;
    float moon_glow = get_square_halo(moon_uv, 0.015, 0.027, 0.0035) * moon_facing_mask;
    float moon_outer_glow = get_square_halo(moon_uv, 0.027, 0.044, 0.008) * moon_facing_mask;
    vec3 moon_core_color = vec3(0.86, 0.89, 0.94);
    vec3 moon_inner_halo_color = vec3(0.50, 0.76, 0.98);
    vec3 moon_outer_halo_color = vec3(0.32, 0.38, 0.78);
    color += moon_outer_halo_color * 0.18 * moon_outer_glow + moon_inner_halo_color * 0.36 * moon_glow + moon_core_color * 0.82 * moon_disc;

    gl_FragDepth = 1.0;
    outColor = vec4(saturate(sanitize_sky_color(color, flat_color)), 1.0);
}
