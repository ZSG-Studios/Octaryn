#version 450

#include "shader_common.glsl"
#include "atlas_config.glsl"
#include "tiled_atlas_sampling.glsl"
#include "pbr_material.glsl"
#include "terrain_parallax.glsl"

layout(set = 2, binding = 0) uniform sampler2DArray atlas_texture;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_normal_texture;
layout(set = 2, binding = 2) uniform sampler2DArray atlas_specular_texture;

layout(set = 3, binding = 0) uniform SkylightUniforms
{
    float SkylightFloor;
    float CloudTimeSeconds;
    float SkyVisibility;
    float TwilightStrength;
    float CelestialVisibility;
    uint MaterialFlags;
    uint _PadMaterial0;
    uint _PadMaterial1;
    vec4 CameraPosition;
};

layout(set = 3, binding = 1) uniform HiddenBlockUniforms
{
    uint HiddenBlockCount;
    ivec3 _HiddenPad;
    ivec4 HiddenBlocks[kHiddenBlockCapacity];
};

layout(location = 0) in vec4 vWorldPosition;
layout(location = 1) flat in vec3 vNormal;
layout(location = 2) in vec3 vTexcoord;
layout(location = 3) flat in uint vVoxel;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outVoxel;
layout(location = 3) out vec4 outMaterial;

const uint kMaterialFlagPbr = 1u << 0u;
const uint kMaterialFlagPom = 1u << 1u;

void main()
{
    vec3 world_position = vWorldPosition.xyz + CameraPosition.xyz;
    outPosition = vec4(vWorldPosition.xyz, vWorldPosition.w);
    outVoxel = encode_voxel_to_rgba8(vVoxel);
    outMaterial = vec4(0.8, 0.0, 0.04, 0.0);

    if (HiddenBlockCount > 0u)
    {
        ivec3 world_block = get_face_owner_world_block_robust(world_position, vNormal, vVoxel);
        if (is_hidden_edited_block(world_block, HiddenBlockCount, HiddenBlocks))
        {
            discard;
        }
    }

    if (is_cloud_voxel(vVoxel))
    {
        float alpha = get_cloud_alpha(world_position, vNormal, CloudTimeSeconds);
        if (alpha < kAtlasAlphaCutoff)
        {
            discard;
        }
        outColor = vec4(get_cloud_color(world_position,
                                        vNormal,
                                        CloudTimeSeconds,
                                        SkyVisibility,
                                        TwilightStrength,
                                        CelestialVisibility),
                        1.0);
        return;
    }

    vec2 face_uv = get_face_sample_uv(world_position, vVoxel, vTexcoord);
    float face_edge_weight = terrain_face_edge_weight(atlas_normal_texture, face_uv);
    vec2 face_uv_dx = dFdx(face_uv);
    vec2 face_uv_dy = dFdy(face_uv);
    vec3 view_delta = CameraPosition.xyz - world_position;
    float view_distance = length(view_delta);
    vec3 view_dir = view_delta / max(view_distance, 0.001);
    bool pbr_enabled = (MaterialFlags & kMaterialFlagPbr) != 0u;
    bool pom_enabled = pbr_enabled && (MaterialFlags & kMaterialFlagPom) != 0u;
    ParallaxSample parallax;
    parallax.uv = face_uv;
    parallax.depth = 0.0;
    parallax.slope_normal = vec3(0.0, 0.0, 1.0);
    parallax.slope_blend = 0.0;
    if (pom_enabled && face_edge_weight > 0.001)
    {
        parallax = apply_terrain_parallax(atlas_normal_texture,
                                          face_uv,
                                          face_uv_dx,
                                          face_uv_dy,
                                          view_dir,
                                          view_distance,
                                          vVoxel);
        face_uv = parallax.uv;
    }
    outColor = is_sprite_voxel(vVoxel) ? sample_sprite_atlas_texture(atlas_texture, vec3(face_uv, vTexcoord.z))
                                       : sample_face_tiled_atlas_grad(atlas_texture, face_uv, vVoxel, face_uv_dx, face_uv_dy);
    outColor = apply_biome_tint(outColor, vVoxel);
    if (!get_occlusion(vVoxel) && outColor.a < kAtlasAlphaCutoff)
    {
        discard;
    }
    outColor.a = get_sky_ambient_factor(vNormal, vVoxel, SkylightFloor, SkyVisibility);
    if (!pbr_enabled)
    {
        return;
    }

    vec4 normal_sample = is_sprite_voxel(vVoxel) ? sample_sprite_atlas_texture(atlas_normal_texture, vec3(face_uv, vTexcoord.z))
                                                 : sample_face_tiled_atlas_grad(atlas_normal_texture, face_uv, vVoxel, face_uv_dx, face_uv_dy);
    vec4 specular_sample = is_sprite_voxel(vVoxel) ? sample_sprite_atlas_texture(atlas_specular_texture, vec3(face_uv, vTexcoord.z))
                                                   : sample_face_tiled_atlas_grad(atlas_specular_texture, face_uv, vVoxel, face_uv_dx, face_uv_dy);
    PbrMaterial material = decode_labpbr_material(normal_sample, specular_sample, normalize(vNormal), vVoxel);
    material.normal = normalize(mix(normalize(vNormal), material.normal, face_edge_weight));
    if (parallax.slope_blend > 0.0 && !is_sprite_voxel(vVoxel))
    {
        vec3 slope_normal = transform_face_tangent_normal(parallax.slope_normal, normalize(vNormal), vVoxel);
        material.normal = normalize(mix(material.normal, slope_normal, parallax.slope_blend * face_edge_weight));
    }
    material.material_ao *= mix(1.0, 0.82, parallax.depth);

    outMaterial = vec4(material.roughness, material.metallic, material.f0, material.emission);
}
