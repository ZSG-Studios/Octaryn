#pragma once
// Recovered shader_common cloud hash, moon visibility, and face lighting.
float get_moon_strength(float sky_visibility, float celestial_visibility)
{
    float moon = saturate(1.0 - celestial_visibility) * saturate(1.0 - sky_visibility * 0.35);
    return moon * sqrt(moon);
}

float2 get_random2(float2 position)
{
    const float2 k1 = float2(127.1, 311.7);
    const float2 k2 = float2(269.5, 183.3);
    float n = sin(dot(position, k1)) * 43758.5453;
    float m = sin(dot(position, k2)) * 43758.5453;
    float2 r = frac(float2(n, m));
    return r * 2.0 - 1.0;
}

float3 get_cloud_color(float3 position,
                     float3 normal,
                     float cloud_time_seconds,
                     float sky_visibility,
                     float twilight_strength,
                     float celestial_visibility)
{
    float daylight = saturate(sky_visibility);
    float dusk = saturate(twilight_strength) * saturate(1.0 - sky_visibility * 0.35);
    float moonlight = get_moon_strength(sky_visibility, celestial_visibility);
    float face_shade = normal.y > 0.5 ? 1.0 : (normal.y < -0.5 ? 0.84 : 0.92);
    float3 midnight_base = float3(0.20, 0.21, 0.24);
    float3 dusk_base = float3(0.68, 0.69, 0.72);
    float3 day_base = float3(0.98, 0.98, 0.99);
    float3 base = lerp(midnight_base, dusk_base, saturate(dusk * 1.25));
    base = lerp(base, day_base, daylight);
    float3 moon_tint = float3(0.20, 0.21, 0.24) * moonlight;
    float3 twilight_tint = float3(0.96, 0.58, 0.36) * dusk * 0.10;
    return saturate(base * face_shade + moon_tint + twilight_tint);
}
