#pragma once
// Exact recovered shader_common sky/fog math. Global control wiring is completed here.
float get_sky_gradient_factor(float3 position)
{
    float dy = position.y;
    float dx = length(position.xz);
    return (atan2(dy, dx) + 3.14159265358979323846 / 2.0) / 3.14159265358979323846;
}

float3 get_sky_color(float3 position)
{
    float alpha = get_sky_gradient_factor(position);
    float upper = smoothstep(0.45, 1.0, alpha);
    float below = smoothstep(0.48, 0.0, alpha);
    float3 horizon = float3(0.72, 0.84, 0.98);
    float3 mid = float3(0.38, 0.64, 0.96);
    float3 zenith = float3(0.14, 0.38, 0.78);
    float3 lower = float3(0.42, 0.58, 0.82);
    float3 upper_sky = lerp(mid, zenith, pow(upper, 1.18));
    return lerp(lerp(horizon, upper_sky, upper), lower, below * 0.46);
}

float3 get_night_sky_color(float3 position)
{
    float alpha = get_sky_gradient_factor(position);
    float upper = smoothstep(0.42, 1.0, alpha);
    float below = smoothstep(0.46, 0.0, alpha);
    float3 horizon = float3(0.026, 0.032, 0.054);
    float3 zenith = float3(0.004, 0.008, 0.020);
    float3 lower = float3(0.012, 0.014, 0.022);
    return lerp(lerp(horizon, zenith, pow(upper, 0.82)), lower, below * 0.56);
}

float3 get_sky_color_lit(float3 position, float sky_visibility, float twilight_strength)
{
    float alpha = get_sky_gradient_factor(position);
    float horizon = 1.0 - abs(alpha * 2.0 - 1.0);
    horizon = pow(saturate(horizon), 2.35);
    float sky = smoothstep(0.03, 1.0, saturate(sky_visibility));
    float twilight = saturate(twilight_strength);
    float upper = smoothstep(0.52, 1.0, alpha);
    float3 night = get_night_sky_color(position);
    float3 day = get_sky_color(position);
    float3 base = lerp(night, day, sky);
    float3 amber_band = float3(1.0, 0.46, 0.16) * horizon * twilight * 0.46;
    float3 rose_band = float3(0.52, 0.20, 0.42) * horizon * twilight * (1.0 - sky) * 0.20;
    float3 upper_twilight = float3(0.15, 0.12, 0.32) * upper * twilight * (1.0 - sky) * 0.34;
    float3 daylight_haze = float3(1.0, 0.94, 0.82) * horizon * sky * 0.055;
    return saturate(base + amber_band + rose_band + upper_twilight + daylight_haze);
}

float get_fog(float x, float distance)
{
    return min(pow(x / distance, 2.5), 1.0);
}

float3 get_fog_color(float3 position, float sky_visibility, float twilight_strength)
{
    float alpha = get_sky_gradient_factor(position);
    float horizon = 1.0 - abs(alpha * 2.0 - 1.0);
    horizon = pow(saturate(horizon), 2.0);
    float3 sky = get_sky_color_lit(position, sky_visibility, twilight_strength);
    float3 night_fog = lerp(float3(0.022, 0.022, 0.026), float3(0.016, 0.016, 0.020), alpha);
    night_fog += float3(0.010, 0.010, 0.012) * horizon * 0.12;
    float3 dusk_fog = float3(0.26, 0.11, 0.05) * horizon * saturate(twilight_strength) * 0.10;
    return lerp(night_fog + dusk_fog, sky * lerp(0.84, 0.96, horizon), saturate(sky_visibility * 0.80 + twilight_strength * 0.35));
}

float apply_fog_factor(float fog, float sky_visibility)
{
    float night = saturate(1.0 - sky_visibility);
    float clear_band = 0.10 * night;
    fog = saturate((fog - clear_band) / max(1.0 - clear_band, 0.0001));
    return lerp(fog, sqrt(fog), night * 0.06);
}


float3 apply_scene_fog(float3 hdr, float3 relative, float sky_visibility,
                       float twilight, bool enabled, float distance) {
    if (!enabled) return hdr;
    float amount=apply_fog_factor(get_fog(length(relative),max(distance,0.001)),sky_visibility);
    return lerp(hdr,get_fog_color(relative,sky_visibility,twilight),amount);
}
