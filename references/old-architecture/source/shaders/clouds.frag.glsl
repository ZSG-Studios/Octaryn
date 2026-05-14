#version 450

layout(set = 3, binding = 1) uniform ProjUniforms { mat4 Proj; };
layout(set = 3, binding = 2) uniform ViewUniforms { mat4 View; };

#include "shader_common.glsl"

layout(set = 3, binding = 0) uniform CloudUniforms {
    vec4 LightDirectionAndSkyVisibility;
    vec4 TwilightCelestialCloudTime;
    vec4 CameraPositionAndCloudHeight;
};

layout(location = 0) in vec3 vLocalPosition;

layout(location = 0) out vec4 outColor;

const float kSkyCloudCellSize = 3.0;
const float kSkyCloudThickness = 1.0;
const float kSkyCloudMoveSpeed = 0.6;
const float kSkyCloudAlpha = 0.72;
const int kSkyCloudMaxSteps = 1024;

vec2 get_shader_cloud_offset(float cloud_time_seconds)
{
    return normalize(vec2(1.0, -0.18)) * cloud_time_seconds * kSkyCloudMoveSpeed;
}

float get_shader_cloud_cell_value(vec2 cell)
{
    return get_random2(cell).x * 0.5 + 0.5;
}

float get_shader_cloud_value_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = get_shader_cloud_cell_value(i);
    float b = get_shader_cloud_cell_value(i + vec2(1.0, 0.0));
    float c = get_shader_cloud_cell_value(i + vec2(0.0, 1.0));
    float d = get_shader_cloud_cell_value(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

bool is_shader_cloud_cell_filled(ivec2 cell)
{
    vec2 c = vec2(cell);
    vec2 warp = vec2(get_shader_cloud_value_noise(c / 43.0 + vec2(17.0, -9.0)),
                     get_shader_cloud_value_noise(c / 39.0 + vec2(-31.0, 14.0))) - vec2(0.5);
    vec2 p = c + warp * 14.0;
    float weather = get_shader_cloud_value_noise(p / 34.0 + vec2(7.0, 11.0));
    if (weather < 0.61)
    {
        return false;
    }

    float body = get_shader_cloud_value_noise(p / 12.0 + vec2(-5.0, 23.0));
    float lobe = get_shader_cloud_value_noise(p / 5.5 + vec2(19.0, -11.0));
    float breakup = get_shader_cloud_cell_value(c + floor(warp * 19.0));
    float density = weather * 0.44 + body * 0.38 + lobe * 0.18 - breakup * 0.08;
    float threshold = mix(0.64, 0.54, smoothstep(0.66, 0.92, weather));
    return density >= threshold;
}

vec4 get_camera_relative_view_position_cloud(mat4 view, vec3 world_position, vec3 camera_position)
{
    mat4 view_rotation = view;
    view_rotation[3][0] = 0.0;
    view_rotation[3][1] = 0.0;
    view_rotation[3][2] = 0.0;
    return view_rotation * vec4(world_position - camera_position, 1.0);
}

bool get_shader_cloud_hit(vec3 camera_position,
                          vec3 ray_direction,
                          float cloud_layer_height,
                          float cloud_time_seconds,
                          float cloud_max_distance,
                          out vec3 hit_position,
                          out vec3 hit_normal,
                          out float hit_distance)
{
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
    vec3 enter_normal = vec3(0.0);

    if (abs(ray_direction.y) > 0.0001)
    {
        float t0 = (cloud_bottom - camera_position.y) / ray_direction.y;
        float t1 = (cloud_top - camera_position.y) / ray_direction.y;
        float near_t = min(t0, t1);
        float far_t = max(t0, t1);
        enter_t = max(near_t, 0.0);
        exit_t = min(far_t, cloud_max_distance);
        enter_normal = t0 < t1 ? vec3(0.0, -1.0, 0.0) : vec3(0.0, 1.0, 0.0);
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
    vec2 cloud_offset = get_shader_cloud_offset(cloud_time_seconds);
    vec2 cloud_position = camera_position.xz + ray_direction.xz * t + cloud_offset;
    ivec2 cell = ivec2(floor(cloud_position / kSkyCloudCellSize));
    ivec2 step_dir = ivec2(ray_direction.x >= 0.0 ? 1 : -1,
                           ray_direction.z >= 0.0 ? 1 : -1);
    vec2 next_boundary = (vec2(cell) + vec2(step_dir.x > 0 ? 1.0 : 0.0,
                                            step_dir.y > 0 ? 1.0 : 0.0)) * kSkyCloudCellSize;
    vec2 t_max = vec2(1.0e20);
    vec2 t_delta = vec2(1.0e20);
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

    vec3 face_normal = enter_normal;
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
                hit_normal = abs(ray_direction.y) > 0.35 ? vec3(0.0, -sign(ray_direction.y), 0.0)
                                                          : normalize(vec3(-ray_direction.x, 0.0, -ray_direction.z));
            }
            return true;
        }

        if (t_max.x < t_max.y)
        {
            t = t_max.x + 0.015;
            t_max.x += t_delta.x;
            cell.x += step_dir.x;
            face_normal = vec3(-float(step_dir.x), 0.0, 0.0);
        }
        else
        {
            t = t_max.y + 0.015;
            t_max.y += t_delta.y;
            cell.y += step_dir.y;
            face_normal = vec3(0.0, 0.0, -float(step_dir.y));
        }
    }

    return false;
}

void main()
{
    vec3 ray_direction = normalize(vLocalPosition);
    vec3 light_direction = LightDirectionAndSkyVisibility.xyz;
    float sky_visibility = LightDirectionAndSkyVisibility.w;
    float twilight_strength = TwilightCelestialCloudTime.x;
    float celestial_visibility = TwilightCelestialCloudTime.y;
    float cloud_time_seconds = TwilightCelestialCloudTime.z;
    float cloud_max_distance = TwilightCelestialCloudTime.w;
    vec3 camera_position = CameraPositionAndCloudHeight.xyz;
    float cloud_layer_height = CameraPositionAndCloudHeight.w;

    vec3 cloud_hit_position = vec3(0.0);
    vec3 cloud_hit_normal = vec3(0.0);
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

    vec3 sun_direction = normalize(-light_direction);
    vec3 moon_direction = -sun_direction;
    float moon_visibility = saturate(get_moon_strength(sky_visibility, celestial_visibility) * 1.10);
    float sun_passthrough = smoothstep(0.88, 0.995, dot(ray_direction, sun_direction)) * saturate(celestial_visibility);
    float moon_passthrough = smoothstep(0.88, 0.995, dot(ray_direction, moon_direction)) * moon_visibility;
    float celestial_passthrough = max(sun_passthrough, moon_passthrough);
    float night_visibility = max(max(saturate(sky_visibility), saturate(twilight_strength) * 0.75),
                                 moon_visibility * 0.55);
    float night_alpha = mix(kSkyCloudAlpha * 0.24, kSkyCloudAlpha, night_visibility);
    float cloud_alpha = mix(night_alpha, night_alpha * 0.58, celestial_passthrough);

    vec3 cloud_color = get_cloud_color(cloud_hit_position,
                                       cloud_hit_normal,
                                       cloud_time_seconds,
                                       sky_visibility,
                                       twilight_strength,
                                       celestial_visibility);
    float low_sun_cloud_tint = (1.0 - smoothstep(0.06, 0.62, sun_direction.y)) *
                               smoothstep(-0.14, 0.10, sun_direction.y) *
                               saturate(twilight_strength + celestial_visibility);
    vec3 low_sun_tint = mix(vec3(1.0), vec3(1.0, 0.48, 0.16), low_sun_cloud_tint * 0.36);
    cloud_color *= low_sun_tint;
    cloud_color = max(cloud_color, vec3(0.075, 0.088, 0.130) * (1.0 - saturate(sky_visibility)));

    vec4 view_position = get_camera_relative_view_position_cloud(View, cloud_hit_position, camera_position);
    vec4 clip_position = Proj * view_position;
    float device_depth = clip_position.z / clip_position.w;
    gl_FragDepth = saturate(device_depth);
    outColor = vec4(cloud_color, cloud_alpha);
}
