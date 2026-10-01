// Volumetric cloud layer: raymarched density slab with a 2D weather map,
// Perlin-Worley style erosion, Beer-Powder lighting and a silver lining.
// Native RHI fullscreen triangle supplies rays; output is alpha-blended.
uniform float4 LightDirectionAndSkyVisibility;
uniform float4 TwilightCelestialCloudTime;
uniform float4 CameraPositionAndCloudHeight;
#include "SkyRay.slang"
#include "Atmosphere.slang"
#ifdef OCTARYN_CLOUD_OCCLUSION
#include "CloudOcclusionLib.slang"
Texture2D<float4> cloudOpaquePositions;
#endif
struct CloudOutput { float4 color : SV_Target; float depth : SV_Depth; };

static const float kCloudThickness = 240.0;      // metres above the layer base
static const float kCloudCoverage = 0.52;
static const float kCloudDensityScale = 0.055;
static const float kCloudWindSpeed = 6.0;
static const int kCloudViewSteps = 44;
static const int kCloudLightSteps = 6;
static const float kCloudLightStride = 26.0;
static const float kCloudMaxTrace = 14000.0;

float cloud_hash(float3 p)
{
    p = frac(p * 0.3183099 + float3(0.1, 0.2, 0.3));
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float cloud_hash2(float2 p)
{
    float3 q = frac(float3(p.xyx) * 0.3183099 + float3(0.11, 0.17, 0.13));
    q *= 19.0;
    return frac(q.x * q.y * q.z * (q.x + q.y + q.z));
}

float cloud_value_noise3(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float a = cloud_hash(i);
    float b = cloud_hash(i + float3(1, 0, 0));
    float c = cloud_hash(i + float3(0, 1, 0));
    float d = cloud_hash(i + float3(1, 1, 0));
    float e = cloud_hash(i + float3(0, 0, 1));
    float g = cloud_hash(i + float3(1, 0, 1));
    float h = cloud_hash(i + float3(0, 1, 1));
    float k = cloud_hash(i + float3(1, 1, 1));
    return lerp(lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y),
                lerp(lerp(e, g, u.x), lerp(h, k, u.x), u.y), u.z);
}

float cloud_fbm3(float3 p, int octaves)
{
    float sum = 0.0;
    float amplitude = 0.5;
    float total = 0.0;
    for (int i = 0; i < octaves; ++i)
    {
        sum += cloud_value_noise3(p) * amplitude;
        total += amplitude;
        p = p * 2.13 + float3(19.7, 7.3, 11.1);
        amplitude *= 0.5;
    }
    return sum / total;
}

// Cheap cellular-Worley approximation over a 2x2x2 neighborhood.
float cloud_worley3(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float best = 8.0;
    for (int z = 0; z <= 1; ++z)
    for (int y = 0; y <= 1; ++y)
    for (int x = 0; x <= 1; ++x)
    {
        float3 cell = i + float3(x, y, z) - 0.5;
        float3 point = float3(cloud_hash(cell + 0.13),
                              cloud_hash(cell + 37.7),
                              cloud_hash(cell + 71.3));
        float3 delta = cell + point - (i + f);
        best = min(best, dot(delta, delta));
    }
    return saturate(sqrt(best));
}

float cloud_weather(float2 xz, float time_seconds)
{
    float2 p = xz * 0.00042 + float2(time_seconds * 0.0016, time_seconds * 0.0004) +
               float2(48.1, 107.3); // Phase chosen for varied coverage near the spawn region.
    float base = cloud_value_noise3(float3(p, 3.1));
    float detail = cloud_value_noise3(float3(p * 3.7 + 11.0, 9.4));
    float weather = base * 0.72 + detail * 0.28;
    return smoothstep(1.10 - kCloudCoverage, 1.44 - kCloudCoverage, weather);
}

float cloud_height_gradient(float height_fraction)
{
    float bottom = smoothstep(0.0, 0.22, height_fraction);
    float top = 1.0 - smoothstep(0.62, 1.0, height_fraction);
    return bottom * top;
}

// Density at a world position; detail erosion is skipped for light samples.
float cloud_density(float3 p, float layer_base, float time_seconds, float weather_hint, bool detail)
{
    float height_fraction = (p.y - layer_base) / kCloudThickness;
    if (height_fraction <= 0.0 || height_fraction >= 1.0)
    {
        return 0.0;
    }
    float2 wind = float2(1.0, 0.24) * (time_seconds * kCloudWindSpeed);
    float3 q = p + float3(wind.x, 0.0, wind.y);
    float weather = weather_hint >= 0.0 ? weather_hint : cloud_weather(p.xz, time_seconds);
    if (weather <= 0.002)
    {
        return 0.0;
    }
    float base_shape = cloud_fbm3(q * 0.0042, 3);
    // Coverage gates the body; the remainder keeps strong shape contrast.
    float density = saturate((base_shape - (1.0 - weather) * 0.72 - 0.10) * 1.5);
    if (detail && density > 0.003)
    {
        float erosion = cloud_worley3(q * 0.016) * 0.55 +
                        cloud_worley3(q * 0.052 + 7.0) * 0.45;
        density *= 0.30 + 0.70 * erosion;
    }
    return density * cloud_height_gradient(height_fraction) * kCloudDensityScale;
}

float cloud_light_transmittance(float3 p, float3 sun_direction, float layer_base, float time_seconds)
{
    float optical_depth = 0.0;
    for (int i = 0; i < kCloudLightSteps; ++i)
    {
        float3 sample_position = p + sun_direction * (kCloudLightStride * (float(i) + 0.5));
        optical_depth += cloud_density(sample_position, layer_base, time_seconds, -1.0, false) *
                         kCloudLightStride;
    }
    return exp(-optical_depth * 0.9);
}

float cloud_bayer_dither(float2 pixel)
{
    int2 cell = int2(int(pixel.x) & 3, int(pixel.y) & 3);
    int index = cell.y * 4 + cell.x;
    static const float bayer[16] = { 0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                     3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0 };
    return (bayer[index] + 0.5) / 16.0;
}

[shader("fragment")]
CloudOutput fragment_main(SkyVertex input)
{
    float3 ray_direction = normalize(input.direction);
    float3 light_direction = LightDirectionAndSkyVisibility.xyz;
    float sky_visibility = LightDirectionAndSkyVisibility.w;
    float twilight_strength = TwilightCelestialCloudTime.x;
    float celestial_visibility = TwilightCelestialCloudTime.y;
    float cloud_time_seconds = TwilightCelestialCloudTime.z;
    float cloud_max_distance = min(TwilightCelestialCloudTime.w, kCloudMaxTrace);
    float3 camera_position = CameraPositionAndCloudHeight.xyz;
    float layer_base = CameraPositionAndCloudHeight.w;
    float layer_top = layer_base + kCloudThickness;

    // Intersect the cloud slab.
    float enter_t = 0.0;
    float exit_t = 0.0;
    if (abs(ray_direction.y) > 0.0005)
    {
        float t0 = (layer_base - camera_position.y) / ray_direction.y;
        float t1 = (layer_top - camera_position.y) / ray_direction.y;
        enter_t = max(min(t0, t1), 0.0);
        exit_t = max(t0, t1);
    }
    else if (camera_position.y > layer_base && camera_position.y < layer_top)
    {
        enter_t = 0.0;
        exit_t = cloud_max_distance;
    }
    exit_t = min(exit_t, cloud_max_distance);
    if (exit_t <= enter_t)
    {
        discard;
    }
#ifdef OCTARYN_CLOUD_OCCLUSION
    uint width, height;
    cloudOpaquePositions.GetDimensions(width, height);
    uint2 pixel = uint2(input.position.xy);
    if (pixel.x < width && pixel.y < height &&
        cloud_occluded_before_entry(cloudOpaquePositions.Load(int3(pixel, 0)), enter_t,
            dot(ray_direction, CameraForward.xyz), Projection.zw))
        discard;
#endif

    float3 sun_direction = get_safe_direction(-light_direction, float3(0.0, 1.0, 0.0));
    float sun_height = sun_direction.y;
    float day = get_smootherstep(-0.10, 0.22, sun_height) * saturate(sky_visibility);
    float night = 1.0 - day;
    float3 sun_color = atmosphere_sun_color(sun_height);

    float dither = cloud_bayer_dither(input.position.xy);
    float step_length = (exit_t - enter_t) / float(kCloudViewSteps);
    float t = enter_t + step_length * dither;

    float cos_theta = dot(ray_direction, sun_direction);
    // Silver lining: forward-scattering lobe plus soft isotropic base.
    float g = 0.42;
    float g2 = g * g;
    float phase = 0.55 + 0.45 * (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5);

    float3 sun_light = sun_color * (2.6 * day + 0.06) * phase;
    float twilight_band = saturate(twilight_strength) * (1.0 - smoothstep(0.10, 0.42, sun_height));
    sun_light += float3(1.0, 0.36, 0.10) * twilight_band * 0.55 * phase;

    float3 accumulated = float3(0.0);
    float transmittance = 1.0;
    float first_hit_distance = 0.0;
    for (int i = 0; i < kCloudViewSteps; ++i)
    {
        if (t >= exit_t || transmittance < 0.015)
        {
            break;
        }
        float3 p = camera_position + ray_direction * t;
        float weather = cloud_weather(p.xz, cloud_time_seconds);
        float density = cloud_density(p, layer_base, cloud_time_seconds, weather, true);
        if (density > 0.0004)
        {
            float height_fraction = saturate((p.y - layer_base) / kCloudThickness);
            float beer = cloud_light_transmittance(p, sun_direction, layer_base, cloud_time_seconds);
            float powder = 1.0 - exp(-density * step_length * 34.0);
            // Ambient: blue sky dome from above, warm ground bounce from below.
            float3 ambient_top = lerp(float3(0.30, 0.42, 0.62), float3(0.55, 0.65, 0.85), day);
            float3 ambient_bottom = lerp(float3(0.10, 0.11, 0.16), float3(0.32, 0.34, 0.40), day);
            float3 ambient = lerp(ambient_bottom, ambient_top, height_fraction) * 1.6 *
                             (0.16 + 0.84 * day) + float3(0.045, 0.055, 0.085) * night;
            ambient += float3(0.55, 0.20, 0.06) * twilight_band * (1.0 - height_fraction) * 0.35;
            float3 lighting = sun_light * beer * powder +
                ambient * (0.30 + 0.70 * beer);
            float alpha = 1.0 - exp(-density * step_length * 8.0);
            accumulated += lighting * alpha * transmittance;
            transmittance *= 1.0 - alpha;
            if (first_hit_distance == 0.0)
            {
                first_hit_distance = t;
            }
        }
        t += step_length;
    }

    float alpha = saturate(1.0 - transmittance);
    if (alpha <= 0.004)
    {
        discard;
    }
    // Distant clouds sink into the atmosphere instead of popping at the cap.
    float distance_fade = exp(-max(first_hit_distance, 0.0) / (kCloudMaxTrace * 0.55));
    alpha *= distance_fade;
    accumulated = max(accumulated, float3(0.030, 0.038, 0.062) * alpha * night);
    if (alpha <= 0.004)
    {
        discard;
    }

    float view_depth = first_hit_distance * dot(ray_direction, CameraForward.xyz);
    CloudOutput result;
    result.depth = saturate(Projection.z - Projection.w / max(view_depth, 0.001));
    result.color = float4(max(accumulated, float3(0.0)), alpha);
    return result;
}
