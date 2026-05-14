#version 450

#include "shader_common.glsl"
#include "atlas_config.glsl"
#include "tiled_atlas_sampling.glsl"

layout(set = 2, binding = 0) uniform sampler2DArray atlas_texture;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_normal_texture;
layout(set = 2, binding = 2) uniform sampler2DArray atlas_specular_texture;

layout(set = 3, binding = 0) uniform TransparentUniforms
{
    float SkylightFloor;
    float WorldTimeSeconds;
    float SkyVisibility;
    float TwilightStrength;
    float CelestialVisibility;
    uint MaterialFlags;
    uint _Pad0;
    uint _Pad1;
    vec4 CameraPosition;
    vec4 LightDirection;
};

layout(location = 0) in vec4 vWorldPosition;
layout(location = 1) flat in vec3 vNormal;
layout(location = 3) flat in uint vVoxel;
layout(location = 2) in vec3 vTexcoord;
layout(location = 7) flat in uint vWaterLevel;
layout(location = 8) flat in uint vWaterFace;
layout(location = 9) in float vWaterFill;
layout(location = 10) flat in uint vWaterFlow;

layout(location = 0) out vec4 outColor;

const uint kMaterialFlagPbr = 1u << 0u;

bool is_water_voxel(uint voxel)
{
    return !is_sky(voxel) && !is_sprite_voxel(voxel) && vWaterFace != 0u;
}

bool is_water_side(uint voxel)
{
    uint direction = get_direction(voxel);
    return direction == kDirectionNorth ||
        direction == kDirectionSouth ||
        direction == kDirectionEast ||
        direction == kDirectionWest;
}

uint water_flow_strength()
{
    return (vWaterFlow >> 4u) & 15u;
}

uint water_atlas_voxel(uint atlas_layer)
{
    uint index_mask = INDEX_MASK << INDEX_OFFSET;
    return (vVoxel & ~index_mask) | (atlas_layer << INDEX_OFFSET);
}

bool is_lava_fluid()
{
    uint layer = uint(vTexcoord.z + 0.5);
    return layer == kAtlasLavaStillLayer || layer == kAtlasLavaFlowLayer;
}

vec2 get_water_face_uv(vec3 world_position, vec3 face_texcoord)
{
    if (get_direction(vVoxel) == kDirectionUp && water_flow_strength() > 0u)
    {
        return face_texcoord.xy;
    }
    return get_unwrapped_world_face_uv(world_position, get_direction(vVoxel));
}

vec4 sample_water_albedo(vec2 uv)
{
    bool moving_top = get_direction(vVoxel) == kDirectionUp && water_flow_strength() > 0u;
    bool side_face = is_water_side(vVoxel);
    bool flowing_face = side_face || moving_top;
    bool lava = is_lava_fluid();
    uint still_voxel = water_atlas_voxel(lava ? kAtlasLavaStillLayer : kAtlasWaterStillLayer);
    uint flow_voxel = water_atlas_voxel(lava ? kAtlasLavaFlowLayer : kAtlasWaterFlowLayer);
    uint water_voxel = flowing_face ? flow_voxel : still_voxel;
    vec4 albedo = sample_face_tiled_atlas_uv(atlas_texture, uv, water_voxel);
    if (moving_top)
    {
        vec4 calm = sample_face_tiled_atlas_uv(atlas_texture, uv, still_voxel);
        float flow_weight = smoothstep(0.10, 0.70, float(water_flow_strength()) / 15.0);
        albedo = mix(calm, albedo, flow_weight);
    }
    albedo.rgb *= side_face ? 0.92 : 1.0;
    return albedo;
}

vec3 get_water_wave_normal(vec2 uv, vec3 geometric_normal)
{
    if ((MaterialFlags & kMaterialFlagPbr) == 0u)
    {
        return normalize(geometric_normal);
    }
    vec4 normal_sample = sample_face_tiled_atlas_uv(atlas_normal_texture, uv, vVoxel);
    vec3 ripple_normal = normalize(vec3((normal_sample.rg * 2.0 - 1.0) * 0.06, 1.0));
    if (abs(geometric_normal.y) > 0.5)
    {
        return normalize(vec3(ripple_normal.x, geometric_normal.y, ripple_normal.y));
    }
    return normalize(geometric_normal + vec3(ripple_normal.x, ripple_normal.y, 0.0) * 0.04);
}

vec3 get_water_sky_reflection(vec3 view_dir, vec3 water_normal)
{
    vec3 reflected = reflect(-view_dir, water_normal);
    reflected.y = max(reflected.y, 0.03);
    return get_sky_color_lit(reflected, SkyVisibility, TwilightStrength);
}

vec4 shade_water(vec4 albedo, vec2 uv, vec3 world_position, vec3 geometric_normal, float view_depth, uint water_level, float water_fill)
{
    bool lava = is_lava_fluid();
    geometric_normal = normalize(geometric_normal);
    if (!gl_FrontFacing)
    {
        geometric_normal = -geometric_normal;
    }
    vec3 view_delta = CameraPosition.xyz - world_position;
    float view_distance = length(view_delta);
    vec3 view_dir = view_delta / max(view_distance, 0.001);
    vec3 normal = get_water_wave_normal(uv, normalize(geometric_normal));
    if (dot(normal, view_dir) < 0.0)
    {
        normal = -normal;
    }
    float up_face = step(0.5, geometric_normal.y);
    float side_face = is_water_side(vVoxel) ? 1.0 : 0.0;
    float distance_fog = get_fog(abs(view_depth), 220.0);
    float level = float(clamp(water_level, 0u, 7u)) / 7.0;
    float level_fill = 1.0 - level * 0.55;
    float fill = mix(max(saturate(water_fill), 0.45), level_fill, 0.15);
    if (lava)
    {
        float glow = 0.40 + 0.18 * sin(WorldTimeSeconds * 1.2 + dot(uv, vec2(4.0, -2.0)));
        vec3 lava_core = mix(albedo.rgb, vec3(1.0, 0.34, 0.04), 0.28);
        vec3 lava_color = lava_core + vec3(1.0, 0.22, 0.02) * glow;
        lava_color = mix(lava_color, get_fog_color(vec3(1.0, 0.18, 0.0), SkyVisibility, TwilightStrength), distance_fog * 0.05);
        return vec4(saturate(lava_color), 1.0);
    }
    float facing = saturate(dot(normal, view_dir));
    float fresnel = pow(1.0 - facing, 5.0);
    vec3 light_dir = normalize(LightDirection.xyz);
    vec3 half_vector = light_dir + view_dir;
    vec3 half_dir = half_vector / max(length(half_vector), 0.001);
    float sun_visibility = saturate(SkyVisibility * CelestialVisibility);
    float specular = pow(saturate(dot(normal, half_dir)), 96.0) * sun_visibility * up_face;
    float smoothness = 0.68;
    if ((MaterialFlags & kMaterialFlagPbr) != 0u)
    {
        smoothness = max(sample_face_tiled_atlas_uv(atlas_specular_texture, uv, vVoxel).r, smoothness);
    }
    specular *= mix(0.05, 0.20, smoothness);
    vec3 absorption = mix(vec3(0.03, 0.18, 0.24), vec3(0.12, 0.38, 0.54), fill);
    vec3 sky_reflection = get_water_sky_reflection(view_dir, normal);
    vec3 night_tint = vec3(0.010, 0.026, 0.036);
    vec3 lit_absorption = mix(night_tint, absorption, saturate(SkyVisibility * 0.82 + 0.18));
    vec3 side_absorption = mix(lit_absorption, vec3(0.04, 0.19, 0.36), 0.35);
    vec3 color = mix(albedo.rgb, mix(lit_absorption, side_absorption, side_face), 0.26);
    color = mix(color, sky_reflection, (0.015 + fresnel * 0.07) * up_face);
    color += vec3(1.0, 0.92, 0.76) * specular;
    color = mix(color, get_fog_color(vec3(0.0, 0.14, 1.0), SkyVisibility, TwilightStrength), distance_fog * 0.03);
    float alpha = mix(0.32, 0.46, fill);
    alpha = mix(alpha, alpha * 0.82, side_face);
    alpha = mix(alpha, 0.62, fresnel * up_face);
    return vec4(saturate(color), saturate(alpha));
}

void main()
{
    vec3 world_position = vWorldPosition.xyz;
    vec2 face_uv = get_face_sample_uv(world_position, vVoxel, vTexcoord);
    if (is_water_voxel(vVoxel))
    {
        face_uv = get_water_face_uv(world_position, vTexcoord);
    }

    vec4 albedo = is_sprite_voxel(vVoxel) ? sample_sprite_atlas_texture(atlas_texture, vec3(face_uv, vTexcoord.z))
                                          : sample_face_tiled_atlas_grad(atlas_texture, face_uv, vVoxel, dFdx(face_uv), dFdy(face_uv));
    if (is_water_voxel(vVoxel))
    {
        albedo = sample_water_albedo(face_uv);
        outColor = shade_water(albedo, face_uv, world_position, vNormal, vWorldPosition.w, vWaterLevel, vWaterFill);
        return;
    }

    outColor = vec4(albedo.rgb, albedo.a);
}
