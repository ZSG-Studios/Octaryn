#pragma once
// Stars, sun disc and moon composed over the physically-based atmosphere.
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
    return lerp(0.55, 1.0, pulse);
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
    float dist = length(offset);
    float aa = min(fwidth(dist) * 0.75, size * 0.28);
    float core = 1.0 - smoothstep(size - aa, size + aa, dist);
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

// Round celestial disc with anti-aliased limb; uv is the tangent-plane
// projection around the body direction (units of radians-ish dot product).
float get_disc_mask(float2 uv, float radius, float softness)
{
    float dist = length(uv);
    float aa = max(softness, fwidth(dist));
    return 1.0 - smoothstep(radius - aa, radius + aa, dist);
}

float get_disc_halo(float2 uv, float radius, float halo_radius, float softness)
{
    float dist = length(uv);
    float soft_body = radius + softness;
    float soft_halo = halo_radius + softness * 1.5;
    float falloff = 1.0 - get_smootherstep(soft_body, soft_halo, dist);
    return falloff * falloff * (3.0 - 2.0 * falloff);
}

// Subtle procedural maria so the moon reads as a body, not a lamp.
float get_moon_maria(float2 uv)
{
    float2 p = uv * 90.0;
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = get_star_hash(float3(i, 7.0));
    float b = get_star_hash(float3(i + float2(1.0, 0.0), 7.0));
    float c = get_star_hash(float3(i + float2(0.0, 1.0), 7.0));
    float d = get_star_hash(float3(i + float2(1.0, 1.0), 7.0));
    float noise = lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
    return 0.82 + 0.18 * smoothstep(0.35, 0.75, noise);
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

    // Sun: atmosphere-attenuated round disc with a tight forward-scatter glow.
    float3 helper = abs(sun_direction.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(helper, sun_direction));
    float3 up = normalize(cross(sun_direction, right));
    float2 sun_uv = float2(dot(sky_direction, right), dot(sky_direction, up));
    float sun_facing = dot(sky_direction, sun_direction);
    float sun_visible_mask = get_smootherstep(-0.10, -0.015, sun_direction.y);
    float sun_facing_mask = smoothstep(0.0, 0.02, sun_facing) * sun_visible_mask * sun_enabled;
    float sun_disc = get_disc_mask(sun_uv, 0.017, 0.0012) * sun_facing_mask;
    float sun_glow = get_disc_halo(sun_uv, 0.017, 0.075, 0.010) * sun_facing_mask;
    float3 sun_disc_radiance = atmosphere_sun_disc(sky_direction, sun_direction, 0.99985);
    float3 sun_tint = atmosphere_sun_color(sun_direction.y);
    // HDR sun: the core blooms through the filmic present curve.
    color += sun_tint * sun_glow * 0.55;
    color = lerp(color, sun_disc_radiance * 0.24, saturate(sun_disc * 0.95));
    color += sun_tint * sun_disc * 1.4;

    // Moon: cool round disc with maria, soft halo, never brighter than the sky.
    float3 moon_helper = abs(moon_direction.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 moon_right = normalize(cross(moon_helper, moon_direction));
    float3 moon_up = normalize(cross(moon_direction, moon_right));
    float2 moon_uv = float2(dot(sky_direction, moon_right), dot(sky_direction, moon_up));
    float moon_facing = dot(sky_direction, moon_direction);
    float moon_visible_mask = get_smootherstep(-0.12, -0.03, moon_direction.y);
    float moon_facing_mask = smoothstep(0.0, 0.02, moon_facing) * moon_visible_mask * moon_enabled;
    float moon_disc = get_disc_mask(moon_uv, 0.0125, 0.0011) * moon_facing_mask;
    float moon_glow = get_disc_halo(moon_uv, 0.0125, 0.046, 0.007) * moon_facing_mask;
    float3 moon_core_color = float3(0.86, 0.89, 0.94) * get_moon_maria(moon_uv);
    float3 moon_halo_color = float3(0.42, 0.58, 0.86);
    float night = 1.0 - get_smootherstep(-0.16, 0.10, sun_direction.y);
    color += moon_halo_color * moon_glow * 0.20 * night +
             moon_core_color * moon_disc * 0.80 * max(night, 0.25);

    return float4(sanitize_sky_color(color, flat_color), 1.0);
}
