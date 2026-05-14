#version 450

#include "shader_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D positionTexture;
layout(set = 0, binding = 2) uniform sampler2D voxelTexture;
layout(set = 0, binding = 3) uniform sampler2D materialTexture;
layout(set = 1, binding = 0, rgba16f) uniform image2D compositeTexture;

layout(set = 2, binding = 0) uniform CompositeUniforms
{
    vec4 SkyVisibilityAndAmbientStrength;
};

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(compositeTexture);
    if (coord.x >= size.x || coord.y >= size.y)
    {
        return;
    }

    vec4 color = texelFetch(colorTexture, coord, 0);
    vec4 position = texelFetch(positionTexture, coord, 0);
    uint voxel = decode_voxel_from_rgba8(texelFetch(voxelTexture, coord, 0));
    if (voxel == 0u && position.w == 0.0)
    {
        vec3 ldr = clamp(color.rgb, vec3(0.0), vec3(0.999));
        imageStore(compositeTexture, coord, vec4(ldr / (vec3(1.0) - ldr), 1.0));
        return;
    }

    vec3 albedo = color.rgb;
    vec4 material = texelFetch(materialTexture, coord, 0);
    float emission = clamp(material.a, 0.0, 8.0);
    float visual_sky_visibility = SkyVisibilityAndAmbientStrength.x;
    float ambient_strength = SkyVisibilityAndAmbientStrength.y;

    float ambient_visibility = color.a;
    vec3 ambient = get_ambient_light(ambient_visibility, visual_sky_visibility) * ambient_strength;
    vec3 emissive = albedo * emission;
    vec3 hdr_color = max(albedo * ambient + emissive, vec3(0.0));
    imageStore(compositeTexture, coord, vec4(hdr_color, 1.0));
}
