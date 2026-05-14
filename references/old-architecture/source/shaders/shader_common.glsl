#include "voxel_common.glsl"

const float kEpsilon = 0.001;
const float kPi = 3.14159265;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 saturate(vec2 value)
{
    return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

bool get_occlusion(uint voxel)
{
    return ((voxel >> OCCLUSION_OFFSET) & OCCLUSION_MASK) != 0u;
}

bool is_sprite_direction(uint direction)
{
    return direction >= kDirectionCount;
}

bool is_sprite_voxel(uint voxel)
{
    return is_sprite_direction(get_direction(voxel));
}

uint get_index(uint voxel)
{
    return (voxel >> INDEX_OFFSET) & INDEX_MASK;
}

bool is_sky(uint voxel)
{
    return voxel == 0u;
}

ivec3 get_face_owner_world_block_robust(vec3 world_position, vec3 normal, uint voxel)
{
    vec3 local_pos = world_position - normal * 0.05;
    uvec3 base_block = get_block_position(voxel);
    vec3 center_dist = vec3(base_block) + 0.5 - local_pos;
    local_pos += sign(center_dist) * 0.01;
    return ivec3(floor(local_pos));
}

ivec3 get_face_owner_world_block(vec3 world_position, vec3 normal)
{
    return ivec3(floor(world_position - normal * 0.01));
}

bool is_hidden_edited_block(ivec3 world_block, uint hidden_block_count, ivec4 hidden_blocks[kHiddenBlockCapacity])
{
    for (uint i = 0u; i < hidden_block_count && i < kHiddenBlockCapacity; ++i)
    {
        if (hidden_blocks[i].x == world_block.x && hidden_blocks[i].y == world_block.y && hidden_blocks[i].z == world_block.z)
        {
            return true;
        }
    }
    return false;
}

bool is_cloud_voxel(uint voxel)
{
    return !is_sky(voxel) && !is_sprite_voxel(voxel) && get_index(voxel) == 9u;
}

vec4 encode_voxel_to_rgba8(uint voxel)
{
    return vec4(float((voxel >> 0u) & 0xFFu),
                float((voxel >> 8u) & 0xFFu),
                float((voxel >> 16u) & 0xFFu),
                float((voxel >> 24u) & 0xFFu)) / 255.0;
}

uint decode_voxel_from_rgba8(vec4 encoded)
{
    uvec4 bytes = uvec4(round(clamp(encoded, vec4(0.0), vec4(1.0)) * 255.0));
    return (bytes.x << 0u)
         | (bytes.y << 8u)
         | (bytes.z << 16u)
         | (bytes.w << 24u);
}

float get_sky_gradient_factor(vec3 position)
{
    float dy = position.y;
    float dx = length(position.xz);
    return (atan(dy, dx) + kPi / 2.0) / kPi;
}

vec3 get_sky_color(vec3 position)
{
    float alpha = get_sky_gradient_factor(position);
    float upper = smoothstep(0.45, 1.0, alpha);
    float below = smoothstep(0.48, 0.0, alpha);
    vec3 horizon = vec3(0.72, 0.84, 0.98);
    vec3 mid = vec3(0.38, 0.64, 0.96);
    vec3 zenith = vec3(0.14, 0.38, 0.78);
    vec3 lower = vec3(0.42, 0.58, 0.82);
    vec3 upper_sky = mix(mid, zenith, pow(upper, 1.18));
    return mix(mix(horizon, upper_sky, upper), lower, below * 0.46);
}

vec3 get_night_sky_color(vec3 position)
{
    float alpha = get_sky_gradient_factor(position);
    float upper = smoothstep(0.42, 1.0, alpha);
    float below = smoothstep(0.46, 0.0, alpha);
    vec3 horizon = vec3(0.026, 0.032, 0.054);
    vec3 zenith = vec3(0.004, 0.008, 0.020);
    vec3 lower = vec3(0.012, 0.014, 0.022);
    return mix(mix(horizon, zenith, pow(upper, 0.82)), lower, below * 0.56);
}

vec3 get_sky_color_lit(vec3 position, float sky_visibility, float twilight_strength)
{
    float alpha = get_sky_gradient_factor(position);
    float horizon = 1.0 - abs(alpha * 2.0 - 1.0);
    horizon = pow(saturate(horizon), 2.35);
    float sky = smoothstep(0.03, 1.0, saturate(sky_visibility));
    float twilight = saturate(twilight_strength);
    float upper = smoothstep(0.52, 1.0, alpha);
    vec3 night = get_night_sky_color(position);
    vec3 day = get_sky_color(position);
    vec3 base = mix(night, day, sky);
    vec3 amber_band = vec3(1.0, 0.46, 0.16) * horizon * twilight * 0.46;
    vec3 rose_band = vec3(0.52, 0.20, 0.42) * horizon * twilight * (1.0 - sky) * 0.20;
    vec3 upper_twilight = vec3(0.15, 0.12, 0.32) * upper * twilight * (1.0 - sky) * 0.34;
    vec3 daylight_haze = vec3(1.0, 0.94, 0.82) * horizon * sky * 0.055;
    return saturate(base + amber_band + rose_band + upper_twilight + daylight_haze);
}

float get_moon_strength(float sky_visibility, float celestial_visibility)
{
    float moon = saturate(1.0 - celestial_visibility) * saturate(1.0 - sky_visibility * 0.35);
    return moon * sqrt(moon);
}

vec3 get_moon_light_color(float sky_visibility, float celestial_visibility, vec3 light_direction, uint voxel, vec3 normal)
{
    float moon_strength = get_moon_strength(sky_visibility, celestial_visibility);
    float n_dot_l = dot(normal, light_direction);
    if (is_sprite_voxel(voxel))
    {
        n_dot_l = abs(n_dot_l);
    }
    n_dot_l = max(n_dot_l, 0.0);
    float moon = n_dot_l * moon_strength * 0.52;
    return vec3(0.30, 0.30, 0.31) * moon;
}

float get_sky_ambient_factor(vec3 normal, uint voxel, float ambient_floor, float sky_visibility)
{
    ambient_floor = saturate(ambient_floor);
    float sky = saturate(sky_visibility);
    if (is_sprite_voxel(voxel))
    {
        return max(ambient_floor, 0.72);
    }

    float up = saturate(normal.y * 0.5 + 0.5);
    float hemisphere = mix(0.18, 1.0, up * (0.65 + up * 0.35));
    float underside = pow(1.0 - up, 1.35);
    float night_fill = pow(1.0 - sky, 0.85) * 0.18;
    hemisphere = saturate(hemisphere + underside * night_fill);
    return ambient_floor + (1.0 - ambient_floor) * hemisphere;
}

float get_fog(float x, float distance)
{
    return min(pow(x / distance, 2.5), 1.0);
}

vec3 get_fog_color(vec3 position, float sky_visibility, float twilight_strength)
{
    float alpha = get_sky_gradient_factor(position);
    float horizon = 1.0 - abs(alpha * 2.0 - 1.0);
    horizon = pow(saturate(horizon), 2.0);
    vec3 sky = get_sky_color_lit(position, sky_visibility, twilight_strength);
    vec3 night_fog = mix(vec3(0.022, 0.022, 0.026), vec3(0.016, 0.016, 0.020), alpha);
    night_fog += vec3(0.010, 0.010, 0.012) * horizon * 0.12;
    vec3 dusk_fog = vec3(0.26, 0.11, 0.05) * horizon * saturate(twilight_strength) * 0.10;
    return mix(night_fog + dusk_fog, sky * mix(0.84, 0.96, horizon), saturate(sky_visibility * 0.80 + twilight_strength * 0.35));
}

float apply_fog_factor(float fog, float sky_visibility)
{
    float night = saturate(1.0 - sky_visibility);
    float clear_band = 0.10 * night;
    fog = saturate((fog - clear_band) / max(1.0 - clear_band, 0.0001));
    return mix(fog, sqrt(fog), night * 0.06);
}

vec2 get_random2(vec2 position)
{
    const vec2 k1 = vec2(127.1, 311.7);
    const vec2 k2 = vec2(269.5, 183.3);
    float n = sin(dot(position, k1)) * 43758.5453;
    float m = sin(dot(position, k2)) * 43758.5453;
    vec2 r = fract(vec2(n, m));
    return r * 2.0 - 1.0;
}

vec2 get_random2(vec3 position)
{
    const vec3 k1 = vec3(127.1, 311.7, 74.7);
    const vec3 k2 = vec3(269.5, 183.3, 246.1);
    float n = sin(dot(position, k1)) * 43758.5453;
    float m = sin(dot(position, k2)) * 43758.5453;
    vec2 r = fract(vec2(n, m));
    return r * 2.0 - 1.0;
}

const int kSkylightChunkWidth = 34;
const int kSkylightChunkDepth = 34;
const int kSkylightWordCount = (kSkylightChunkWidth * 256 * kSkylightChunkDepth) / 4;

uint get_skylight_index_from_block(ivec3 block)
{
    return 0u;
}

float get_skylight_at(uint packed_words[kSkylightWordCount], ivec3 block)
{
    return 1.0;
}

float get_skylight(uint packed_words[kSkylightWordCount], uint voxel)
{
    return 1.0;
}

float get_ambient_skylight(uint packed_words[kSkylightWordCount], uint voxel)
{
    return 1.0;
}

float get_ambient_sky_scale(float sky_visibility)
{
    float visibility = max(saturate(sky_visibility), 0.42);
    return mix(0.34, 1.0, sqrt(visibility));
}

vec3 get_ambient_light(float ambient_visibility, float sky_visibility)
{
    float raw_visibility = saturate(ambient_visibility);
    float visibility = pow(raw_visibility, 1.08);
    float sky = max(saturate(sky_visibility), 0.42);
    float local_sky = smoothstep(0.0, 0.02, raw_visibility);
    visibility *= local_sky;
    visibility = max(visibility, mix(0.0, 0.34, sky) * local_sky);
    float sky_scale = get_ambient_sky_scale(sky);
    float day_curve = sqrt(sky);
    vec3 night = vec3(0.090, 0.090, 0.094);
    vec3 day = vec3(0.235, 0.240, 0.245);
    return mix(night, day, day_curve) * visibility * sky_scale;
}

float get_cloud_cell_value(vec2 cell)
{
    return get_random2(cell).x * 0.5 + 0.5;
}

const float kCloudCellSize = 24.0;

vec2 get_cloud_block_uv(vec3 position, float cloud_time_seconds)
{
    vec2 wind = normalize(vec2(1.0, -0.18));
    const float drift = 0.025;
    return floor(position.xz) + wind * cloud_time_seconds * drift;
}

float sample_cloud_density(vec2 uv, float cloud_time_seconds)
{
    float density = get_cloud_cell_value(floor(uv / kCloudCellSize));
    return density >= 0.52 ? 1.0 : 0.0;
}

float get_cloud_alpha(vec3 position, vec3 normal, float cloud_time_seconds)
{
    return sample_cloud_density(get_cloud_block_uv(position, cloud_time_seconds), cloud_time_seconds);
}

vec3 get_cloud_color(vec3 position,
                     vec3 normal,
                     float cloud_time_seconds,
                     float sky_visibility,
                     float twilight_strength,
                     float celestial_visibility);

vec3 get_cloud_color(vec3 position,
                     vec3 normal,
                     float cloud_time_seconds,
                     float sky_visibility,
                     float twilight_strength,
                     float celestial_visibility)
{
    float daylight = saturate(sky_visibility);
    float dusk = saturate(twilight_strength) * saturate(1.0 - sky_visibility * 0.35);
    float moonlight = get_moon_strength(sky_visibility, celestial_visibility);
    float face_shade = normal.y > 0.5 ? 1.0 : (normal.y < -0.5 ? 0.84 : 0.92);
    vec3 midnight_base = vec3(0.20, 0.21, 0.24);
    vec3 dusk_base = vec3(0.68, 0.69, 0.72);
    vec3 day_base = vec3(0.98, 0.98, 0.99);
    vec3 base = mix(midnight_base, dusk_base, saturate(dusk * 1.25));
    base = mix(base, day_base, daylight);
    vec3 moon_tint = vec3(0.20, 0.21, 0.24) * moonlight;
    vec3 twilight_tint = vec3(0.96, 0.58, 0.36) * dusk * 0.10;
    return saturate(base * face_shade + moon_tint + twilight_tint);
}
