#pragma once
// Physically-based analytic atmosphere: raymarched single scattering with a
// multiple-scattering lift, Rayleigh + Mie (Henyey-Greenstein) phase and an
// ozone-style horizon absorption. LUT-free; evaluated per sky pixel.

float get_smootherstep(float edge0, float edge1, float value)
{
    float t = saturate((value - edge0) / max(edge1 - edge0, 0.000001));
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
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

static const float kAtmoPlanetRadiusKm = 6360.0;
static const float kAtmoShellRadiusKm = 6460.0;
static const float3 kAtmoRayleighBeta = float3(5.8e-3, 13.5e-3, 33.1e-3); // 1/km
static const float kAtmoMieBeta = 3.9e-3; // 1/km, clear-air aerosols
static const float kAtmoRayleighHeight = 8.0;  // km scale height
static const float kAtmoMieHeight = 1.2;       // km scale height
static const int kAtmoViewSteps = 16;
static const int kAtmoLightSteps = 6;

float atmo_ray_exit(float3 origin, float3 direction, float radius)
{
    float b = dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - c;
    return discriminant > 0.0 ? -b + sqrt(discriminant) : 0.0;
}

// Transmittance from a point toward the sun through the shell.
float3 atmo_sun_transmittance(float3 position, float3 sun_direction)
{
    if (sun_direction.y < -0.02)
    {
        return float3(0.0);
    }
    float exit_t = atmo_ray_exit(position, sun_direction, kAtmoShellRadiusKm);
    if (exit_t <= 0.0)
    {
        return float3(0.0);
    }
    float step_km = exit_t / float(kAtmoLightSteps);
    float rayleigh_depth = 0.0;
    float mie_depth = 0.0;
    for (int i = 0; i < kAtmoLightSteps; ++i)
    {
        float3 sample_position = position + sun_direction * (step_km * (float(i) + 0.5));
        float height = max(length(sample_position) - kAtmoPlanetRadiusKm, 0.0);
        rayleigh_depth += exp(-height / kAtmoRayleighHeight) * step_km;
        mie_depth += exp(-height / kAtmoMieHeight) * step_km;
    }
    float3 extinction = kAtmoRayleighBeta * rayleigh_depth + kAtmoMieBeta * mie_depth * 1.1;
    // Broad horizon absorption keeps twilight red instead of magenta.
    extinction += float3(0.0018, 0.0009, 0.0002) * mie_depth * mie_depth * 0.05;
    return exp(-extinction);
}

// Linear sky radiance along a view ray from ground level.
float3 atmosphere_radiance(float3 view_direction, float3 sun_direction)
{
    float3 origin = float3(0.0, kAtmoPlanetRadiusKm + 0.002, 0.0);
    float exit_t = atmo_ray_exit(origin, view_direction, kAtmoShellRadiusKm);
    // Below the horizon the ray hits the planet: return deep ground haze.
    float ground_t = view_direction.y < -0.0001
        ? atmo_ray_exit(origin, view_direction, kAtmoPlanetRadiusKm - 0.4) : 0.0;
    bool hits_ground = view_direction.y < -0.0001;
    if (hits_ground)
    {
        exit_t = min(exit_t, max(ground_t, 0.0));
    }

    float cos_theta = dot(view_direction, sun_direction);
    float phase_rayleigh = 3.0 / (16.0 * 3.14159265) * (1.0 + cos_theta * cos_theta);
    float g = 0.76;
    float g2 = g * g;
    float phase_mie = (1.0 - g2) / (4.0 * 3.14159265 * pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));

    float step_km = exit_t / float(kAtmoViewSteps);
    float rayleigh_view_depth = 0.0;
    float mie_view_depth = 0.0;
    float3 scatter_rayleigh = float3(0.0);
    float3 scatter_mie = float3(0.0);
    for (int i = 0; i < kAtmoViewSteps; ++i)
    {
        float3 sample_position = origin + view_direction * (step_km * (float(i) + 0.5));
        float height = max(length(sample_position) - kAtmoPlanetRadiusKm, 0.0);
        float density_rayleigh = exp(-height / kAtmoRayleighHeight) * step_km;
        float density_mie = exp(-height / kAtmoMieHeight) * step_km;
        rayleigh_view_depth += density_rayleigh;
        mie_view_depth += density_mie;
        float3 view_transmittance = exp(-(kAtmoRayleighBeta * rayleigh_view_depth +
                                          kAtmoMieBeta * mie_view_depth * 1.1));
        float3 sun_transmittance = atmo_sun_transmittance(sample_position, sun_direction);
        scatter_rayleigh += view_transmittance * sun_transmittance * density_rayleigh;
        scatter_mie += view_transmittance * sun_transmittance * density_mie;
    }

    const float sun_irradiance = 20.0;
    float3 radiance = sun_irradiance *
        (scatter_rayleigh * kAtmoRayleighBeta * phase_rayleigh +
         scatter_mie * kAtmoMieBeta * phase_mie);
    // Multiple-scattering lift: keeps the sky from going muddy near sunset and
    // fills the anti-solar hemisphere without a second integral.
    float3 multi = sun_irradiance * scatter_rayleigh * kAtmoRayleighBeta *
                   (0.0596831 + 0.028 * saturate(sun_direction.y));
    radiance += multi;
    if (hits_ground)
    {
        radiance = lerp(radiance, float3(0.008, 0.009, 0.011), 0.85);
    }
    return radiance;
}

// Linear radiance of the solar disc, attenuated through the atmosphere.
float3 atmosphere_sun_disc(float3 view_direction, float3 sun_direction, float disc_cosine)
{
    if (dot(view_direction, sun_direction) < disc_cosine)
    {
        return float3(0.0);
    }
    float3 origin = float3(0.0, kAtmoPlanetRadiusKm + 0.002, 0.0);
    return atmo_sun_transmittance(origin, sun_direction) * 42.0;
}

// Sun white-point by height: drives sky lighting and cloud tinting.
float3 atmosphere_sun_color(float sun_height)
{
    float3 origin = float3(0.0, kAtmoPlanetRadiusKm + 0.002, 0.0);
    float3 t = atmo_sun_transmittance(origin, float3(0.0, max(sun_height, 0.0), sqrt(max(1.0 - sun_height * sun_height, 0.0))));
    float3 color = t / max(max(t.r, t.g), max(t.b, 0.0001));
    return lerp(float3(1.0, 0.30, 0.05), color, smoothstep(-0.05, 0.35, sun_height));
}

// Display-range sky color used by the sky pass. The composite resolves the
// LDR value into HDR, so the response stays in [0, 1) with a soft shoulder.
float3 get_atmosphere_sky_color(float3 sky_direction,
                              float3 sun_direction,
                              float sky_visibility,
                              float twilight_strength,
                              float celestial_visibility)
{
    float3 radiance = atmosphere_radiance(sky_direction, sun_direction);
    // Weather/visibility dimmer: overcast and night flatten toward the dark end.
    float visibility = saturate(sky_visibility);
    float day = get_smootherstep(-0.08, 0.20, sun_direction.y) * visibility;
    float3 night_floor = lerp(float3(0.012, 0.018, 0.048), float3(0.016, 0.024, 0.056),
                              1.0 - saturate(abs(sky_direction.y)));
    float3 display = 1.0 - exp(-radiance * 3.1);
    display = max(display * lerp(0.35, 1.0, day), night_floor * (1.0 - day));
    // Deepen and saturate: AAA skies carry a rich zenith, not a pale wash.
    float luma = dot(display, float3(0.2126, 0.7152, 0.0722));
    display = lerp(float3(luma), display, 1.38) * lerp(0.9, 0.78, day);
    // Gentle twilight warmth boost near the sun line, matching old art intent.
    float horizon = pow(1.0 - saturate(abs(sky_direction.y)), 3.0);
    float twilight = saturate(twilight_strength);
    display += float3(0.34, 0.10, 0.03) * horizon * twilight * 0.30;
    return saturate(sanitize_sky_color(display, night_floor));
}

float3 get_flat_sky_color(float3 sun_direction,
                        float sky_visibility,
                        float twilight_strength,
                        float celestial_visibility)
{
    float3 horizon_color = get_atmosphere_sky_color(float3(0.0, 0.02, 1.0),
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
