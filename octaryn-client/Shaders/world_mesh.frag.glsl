#version 450

#include "shader_common.glsl"
#include "atlas_config.glsl"
#include "tiled_atlas_sampling.glsl"

layout(set = 2, binding = 0) uniform sampler2DArray atlas_texture;

layout(set = 3, binding = 0) uniform WorldUniforms
{
    vec4 LightDirectionAndSkyVisibility;
    vec4 TwilightCelestialCloudTime;
    vec4 CameraPositionAndCloudHeight;
    vec4 CelestialToggles;
};

layout(location = 0) in vec4 vWorldPosition;
layout(location = 1) flat in vec3 vNormal;
layout(location = 2) in vec3 vTexcoord;
layout(location = 3) flat in uint vVoxel;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 albedo = sample_face_tiled_atlas_texture(atlas_texture, vWorldPosition.xyz, vVoxel, vTexcoord);
    if (albedo.a < kAtlasAlphaCutoff)
    {
        discard;
    }

    vec3 sun_direction = normalize(-LightDirectionAndSkyVisibility.xyz);
    float sky_visibility = saturate(LightDirectionAndSkyVisibility.w);
    float twilight_strength = saturate(TwilightCelestialCloudTime.x);
    float ndotl = saturate(dot(normalize(vNormal), sun_direction));
    float ambient = mix(0.24, 0.58, sky_visibility) + twilight_strength * 0.12;
    float diffuse = mix(0.18, 0.78, sky_visibility) * ndotl;
    vec3 color = albedo.rgb * saturate(ambient + diffuse);
    outColor = vec4(color, 1.0);
}
