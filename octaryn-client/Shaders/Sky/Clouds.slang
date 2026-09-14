// Original clouds.frag raymarch and coloring; native RHI fullscreen triangle supplies rays.
uniform float4 LightDirectionAndSkyVisibility;
uniform float4 TwilightCelestialCloudTime;
uniform float4 CameraPositionAndCloudHeight;
#include "SkyRay.slang"
#include "CloudColor.slang"
struct CloudOutput { float4 color : SV_Target; float depth : SV_Depth; };
static const float kSkyCloudCellSize = 3.0;
static const float kSkyCloudThickness = 1.0;
static const float kSkyCloudMoveSpeed = 0.6;
static const float kSkyCloudAlpha = 0.72;
static const int kSkyCloudMaxSteps = 1024;

float2 get_shader_cloud_offset(float cloud_time_seconds)
{
    return normalize(float2(1.0, -0.18)) * cloud_time_seconds * kSkyCloudMoveSpeed;
}

float get_shader_cloud_cell_value(float2 cell)
{
    return get_random2(cell).x * 0.5 + 0.5;
}

float get_shader_cloud_value_noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = get_shader_cloud_cell_value(i);
    float b = get_shader_cloud_cell_value(i + float2(1.0, 0.0));
    float c = get_shader_cloud_cell_value(i + float2(0.0, 1.0));
    float d = get_shader_cloud_cell_value(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

bool is_shader_cloud_cell_filled(int2 cell)
{
    float2 c = float2(cell);
    float2 warp = float2(get_shader_cloud_value_noise(c / 43.0 + float2(17.0, -9.0)),
                     get_shader_cloud_value_noise(c / 39.0 + float2(-31.0, 14.0))) - float2(0.5);
    float2 p = c + warp * 14.0;
    float weather = get_shader_cloud_value_noise(p / 34.0 + float2(7.0, 11.0));
    if (weather < 0.61)
    {
        return false;
    }

    float body = get_shader_cloud_value_noise(p / 12.0 + float2(-5.0, 23.0));
    float lobe = get_shader_cloud_value_noise(p / 5.5 + float2(19.0, -11.0));
    float breakup = get_shader_cloud_cell_value(c + floor(warp * 19.0));
    float density = weather * 0.44 + body * 0.38 + lobe * 0.18 - breakup * 0.08;
    float threshold = lerp(0.64, 0.54, smoothstep(0.66, 0.92, weather));
    return density >= threshold;
}

bool get_shader_cloud_hit(float3 camera_position,
                          float3 ray_direction,
                          float cloud_layer_height,
                          float cloud_time_seconds,
                          float cloud_max_distance,
                          out float3 hit_position,
                          out float3 hit_normal,
                          out float hit_distance)
{
    hit_position=float3(0);
    hit_normal=float3(0);
    hit_distance=0;
    float cloud_bottom = cloud_layer_height;
    float cloud_top = cloud_layer_height + kSkyCloudThickness;
    bool below_clouds = camera_position.y < cloud_bottom;
    bool above_clouds = camera_position.y > cloud_top;
    if ((below_clouds && ray_direction.y <= 0.0) ||
        (above_clouds && ray_direction.y >= 0.0))
    {
        return false;
    }

    float enter_t = 0.0;
    float exit_t = cloud_max_distance;
    float3 enter_normal = float3(0.0);

    if (abs(ray_direction.y) > 0.0001)
    {
        float t0 = (cloud_bottom - camera_position.y) / ray_direction.y;
        float t1 = (cloud_top - camera_position.y) / ray_direction.y;
        float near_t = min(t0, t1);
        float far_t = max(t0, t1);
        enter_t = max(near_t, 0.0);
        exit_t = min(far_t, cloud_max_distance);
        enter_normal = t0 < t1 ? float3(0.0, -1.0, 0.0) : float3(0.0, 1.0, 0.0);
    }
    else if (camera_position.y < cloud_bottom || camera_position.y > cloud_top)
    {
        return false;
    }

    if (exit_t <= enter_t)
    {
        return false;
    }

    float t = enter_t + 0.015;
    float2 cloud_offset = get_shader_cloud_offset(cloud_time_seconds);
    float2 cloud_position = camera_position.xz + ray_direction.xz * t + cloud_offset;
    int2 cell = int2(floor(cloud_position / kSkyCloudCellSize));
    int2 step_dir = int2(ray_direction.x >= 0.0 ? 1 : -1,
                           ray_direction.z >= 0.0 ? 1 : -1);
    float2 next_boundary = (float2(cell) + float2(step_dir.x > 0 ? 1.0 : 0.0,
                                            step_dir.y > 0 ? 1.0 : 0.0)) * kSkyCloudCellSize;
    float2 t_max = float2(1.0e20);
    float2 t_delta = float2(1.0e20);
    if (abs(ray_direction.x) > 0.0001)
    {
        t_max.x = t + (next_boundary.x - cloud_position.x) / ray_direction.x;
        t_delta.x = kSkyCloudCellSize / abs(ray_direction.x);
    }
    if (abs(ray_direction.z) > 0.0001)
    {
        t_max.y = t + (next_boundary.y - cloud_position.y) / ray_direction.z;
        t_delta.y = kSkyCloudCellSize / abs(ray_direction.z);
    }

    float3 face_normal = enter_normal;
    int max_steps = min(kSkyCloudMaxSteps, int(ceil(cloud_max_distance / kSkyCloudCellSize)) + 4);
    for (int i = 0; i < max_steps; ++i)
    {
        if (t > exit_t)
        {
            break;
        }

        if (is_shader_cloud_cell_filled(cell))
        {
            hit_distance = t;
            hit_position = camera_position + ray_direction * t;
            hit_normal = face_normal;
            if (dot(hit_normal, hit_normal) < 0.5)
            {
                hit_normal = abs(ray_direction.y) > 0.35 ? float3(0.0, -sign(ray_direction.y), 0.0)
                                                          : normalize(float3(-ray_direction.x, 0.0, -ray_direction.z));
            }
            return true;
        }

        if (t_max.x < t_max.y)
        {
            t = t_max.x + 0.015;
            t_max.x += t_delta.x;
            cell.x += step_dir.x;
            face_normal = float3(-float(step_dir.x), 0.0, 0.0);
        }
        else
        {
            t = t_max.y + 0.015;
            t_max.y += t_delta.y;
            cell.y += step_dir.y;
            face_normal = float3(0.0, 0.0, -float(step_dir.y));
        }
    }

    return false;
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
    float cloud_max_distance = TwilightCelestialCloudTime.w;
    float3 camera_position = CameraPositionAndCloudHeight.xyz;
    float cloud_layer_height = CameraPositionAndCloudHeight.w;

    float3 cloud_hit_position = float3(0.0);
    float3 cloud_hit_normal = float3(0.0);
    float cloud_hit_distance = 0.0;
    if (!get_shader_cloud_hit(camera_position,
                              ray_direction,
                              cloud_layer_height,
                              cloud_time_seconds,
                              cloud_max_distance,
                              cloud_hit_position,
                              cloud_hit_normal,
                              cloud_hit_distance))
    {
        discard;
    }

    float3 sun_direction = normalize(-light_direction);
    float3 moon_direction = -sun_direction;
    float moon_visibility = saturate(get_moon_strength(sky_visibility, celestial_visibility) * 1.10);
    float sun_passthrough = smoothstep(0.88, 0.995, dot(ray_direction, sun_direction)) * saturate(celestial_visibility);
    float moon_passthrough = smoothstep(0.88, 0.995, dot(ray_direction, moon_direction)) * moon_visibility;
    float celestial_passthrough = max(sun_passthrough, moon_passthrough);
    float night_visibility = max(max(saturate(sky_visibility), saturate(twilight_strength) * 0.75),
                                 moon_visibility * 0.55);
    float night_alpha = lerp(kSkyCloudAlpha * 0.24, kSkyCloudAlpha, night_visibility);
    float cloud_alpha = lerp(night_alpha, night_alpha * 0.58, celestial_passthrough);

    float3 cloud_color = get_cloud_color(cloud_hit_position,
                                       cloud_hit_normal,
                                       cloud_time_seconds,
                                       sky_visibility,
                                       twilight_strength,
                                       celestial_visibility);
    float low_sun_cloud_tint = (1.0 - smoothstep(0.06, 0.62, sun_direction.y)) *
                               smoothstep(-0.14, 0.10, sun_direction.y) *
                               saturate(twilight_strength + celestial_visibility);
    float3 low_sun_tint = lerp(float3(1.0), float3(1.0, 0.48, 0.16), low_sun_cloud_tint * 0.36);
    cloud_color *= low_sun_tint;
    cloud_color = max(cloud_color, float3(0.075, 0.088, 0.130) * (1.0 - saturate(sky_visibility)));

    float view_depth = cloud_hit_distance * dot(ray_direction, CameraForward.xyz);
    CloudOutput result;
    result.depth = saturate(Projection.z - Projection.w / view_depth);
    result.color = float4(cloud_color, cloud_alpha);
    return result;
}
