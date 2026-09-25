#pragma once
// Original sky.frag.glsl stars and celestial composition; included by Sky.slang.
float get_star_hash(float3 value)
{
    value = frac(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return frac((value.x + value.y) * value.z);
}

float3 get_star_random(float3 value)
{
    return float3(get_star_hash(value + float3(11.0, 31.0, 57.0)),
                get_star_hash(value + float3(71.0, 19.0, 43.0)),
                get_star_hash(value + float3(37.0, 83.0, 17.0)));
}

float2 get_star_projection(float3 direction)
{
    float hemisphere_scale = max(direction.y + 1.18, 0.24);
    return direction.xz / hemisphere_scale;
}

float get_star_fade(float3 cell, float time_seconds)
{
    float phase = get_star_hash(cell + float3(101.0, 7.0, 61.0));
    float speed = lerp(0.055, 0.185, get_star_hash(cell + float3(3.0, 97.0, 41.0)));
    float cycle = frac(time_seconds * speed + phase);
    float pulse = 1.0 - abs(cycle * 2.0 - 1.0);
    pulse = pulse * pulse * (3.0 - 2.0 * pulse);
    return lerp(0.18, 1.0, pulse);
}

float3 get_star_layer(float3 sky_direction, float cells, float density_threshold, float base_size, float layer_seed, float time_seconds)
{
    float2 projected = get_star_projection(sky_direction);
    float2 grid_position = projected * cells + float2(layer_seed * 0.131, layer_seed * 0.071);
    float2 cell = floor(grid_position);
    float2 cell_uv = frac(grid_position);
    float3 hash_cell = float3(cell, layer_seed);
    float roll = get_star_hash(hash_cell + float3(5.0, 13.0, 29.0));
    if (roll < density_threshold)
    {
        return float3(0.0);
    }

    float3 random = get_star_random(hash_cell);
    float2 star_position = lerp(float2(0.22), float2(0.78), random.xy);
    float2 offset = cell_uv - star_position;
    float size = base_size * lerp(0.70, 1.20, get_star_hash(hash_cell + float3(23.0, 47.0, 89.0)));
    float square_distance = max(abs(offset.x), abs(offset.y));
    float aa = min(fwidth(square_distance) * 0.75, size * 0.28);
    float core = 1.0 - smoothstep(size - aa, size + aa, square_distance);
    float fade = get_star_fade(hash_cell, time_seconds);
    float brightness = lerp(0.62, 1.25, get_star_hash(hash_cell + float3(59.0, 2.0, 109.0)));
    float3 tint = lerp(float3(0.72, 0.82, 1.0), float3(1.0, 0.94, 0.76), get_star_hash(hash_cell + float3(17.0, 67.0, 11.0)));
    return tint * core * brightness * fade;
}

float3 get_stars(float3 sky_direction, float3 moon_direction, float sun_height, float celestial_visibility, float time_seconds)
{
    float horizon_mask = get_smootherstep(-0.04, 0.16, sky_direction.y);
    float night_mask = 1.0 - get_smootherstep(-0.16, 0.08, sun_height);
    float moon_glare = 1.0 - 0.55 * smoothstep(0.84, 0.995, dot(sky_direction, moon_direction));
    float visibility = saturate((1.0 - celestial_visibility) * horizon_mask * night_mask * moon_glare);
    if (visibility <= 0.001)
    {
        return float3(0.0);
    }

    float3 primary = get_star_layer(sky_direction, 82.0, 0.972, 0.048, 17.0, time_seconds);
    float3 secondary = get_star_layer(sky_direction, 137.0, 0.986, 0.038, 53.0, time_seconds + 19.7);
    return (primary + secondary * 0.74) * visibility * 1.90;
}

float4 evaluate_original_sky(float3 vLocalPosition)
{
    float3 sky_direction = get_safe_direction(vLocalPosition, float3(0.0, 1.0, 0.0));
    float3 light_direction = LightDirectionAndSkyVisibility.xyz;
    float sky_visibility = LightDirectionAndSkyVisibility.w;
    float twilight_strength = TwilightCelestialCloudTime.x;
    float celestial_visibility = TwilightCelestialCloudTime.y;
    float sky_gradient_enabled = TwilightCelestialCloudTime.z;
    float star_time_seconds = TwilightCelestialCloudTime.w;
    float stars_enabled = step(0.5, CelestialToggles.x);
    float sun_enabled = step(0.5, CelestialToggles.y);
    float moon_enabled = step(0.5, CelestialToggles.z);

    float3 sun_direction = get_safe_direction(-light_direction, float3(0.0, -1.0, 0.0));
    float3 flat_color = float3(0.0);
    float3 color = float3(0.0);
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
    float3 moon_direction = -sun_direction;
    if (stars_enabled > 0.5)
    {
        color += get_stars(sky_direction, moon_direction, sun_direction.y, celestial_visibility, star_time_seconds);
    }

    float3 helper = abs(sun_direction.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(helper, sun_direction));
    float3 up = normalize(cross(sun_direction, right));
    float2 sun_uv = float2(dot(sky_direction, right), dot(sky_direction, up));
    float sun_facing = dot(sky_direction, sun_direction);
    float sun_visible_mask = get_smootherstep(-0.18, -0.06, sun_direction.y);
    float sun_facing_mask = smoothstep(0.0, 0.02, sun_facing) * sun_visible_mask * sun_enabled;
    float sun_square = get_square_mask(sun_uv, 0.056, 0.0022) * sun_facing_mask;
    float sun_glow = get_square_halo(sun_uv, 0.056, 0.084, 0.009) * sun_facing_mask;
    float sun_outer_glow = get_square_halo(sun_uv, 0.084, 0.118, 0.016) * sun_facing_mask;
    float3 sun_core_color = get_sun_core_color(sun_direction.y);
    float3 sun_halo_color = get_sun_halo_color(sun_direction.y);
    float3 sun_inner_halo_color = lerp(sun_halo_color, float3(1.0, 0.62, 0.16), 0.42);
    float3 sun_outer_halo_color = lerp(sun_halo_color, float3(1.0, 0.24, 0.035), 0.58);
    color += sun_outer_halo_color * 0.34 * sun_outer_glow +
             sun_inner_halo_color * 0.48 * sun_glow;
    color = lerp(color, sun_core_color * 1.45, saturate(sun_square * 0.95));
    color += sun_core_color * 0.55 * sun_square;

    float3 moon_helper = abs(moon_direction.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 moon_right = normalize(cross(moon_helper, moon_direction));
    float3 moon_up = normalize(cross(moon_direction, moon_right));
    float2 moon_uv = float2(dot(sky_direction, moon_right), dot(sky_direction, moon_up));
    float moon_facing = dot(sky_direction, moon_direction);
    float moon_visible_mask = get_smootherstep(-0.18, -0.06, moon_direction.y);
    float moon_facing_mask = smoothstep(0.0, 0.02, moon_facing) * moon_visible_mask * moon_enabled;
    float moon_disc = get_square_mask(moon_uv, 0.015, 0.0012) * moon_facing_mask;
    float moon_glow = get_square_halo(moon_uv, 0.015, 0.027, 0.0035) * moon_facing_mask;
    float moon_outer_glow = get_square_halo(moon_uv, 0.027, 0.044, 0.008) * moon_facing_mask;
    float3 moon_core_color = float3(0.86, 0.89, 0.94);
    float3 moon_inner_halo_color = float3(0.50, 0.76, 0.98);
    float3 moon_outer_halo_color = float3(0.32, 0.38, 0.78);
    color += moon_outer_halo_color * 0.18 * moon_outer_glow + moon_inner_halo_color * 0.36 * moon_glow + moon_core_color * 0.82 * moon_disc;

    return float4(saturate(sanitize_sky_color(color, flat_color)), 1.0);
}
