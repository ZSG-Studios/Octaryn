#version 450

layout(set = 2, binding = 0) uniform sampler2D sceneTexture;
layout(set = 2, binding = 1) uniform sampler2D overlayTexture;
layout(set = 3, binding = 0) uniform PresentUniforms { uint OverlayEnabled; };

layout(location = 0) in vec2 vTexcoord;
layout(location = 0) out vec4 outColor;

vec3 linear_to_srgb(vec3 color)
{
    bvec3 cutoff = lessThanEqual(color, vec3(0.0031308));
    vec3 low = color * 12.92;
    vec3 high = 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, vec3(cutoff));
}

vec3 tone_map(vec3 color)
{
    color = max(color, vec3(0.0));
    return color / (color + vec3(1.0));
}

void main()
{
    vec3 scene = linear_to_srgb(tone_map(texture(sceneTexture, vTexcoord).rgb));
    if (OverlayEnabled == 0u)
    {
        outColor = vec4(scene, 1.0);
        return;
    }
    vec4 overlay = texture(overlayTexture, vTexcoord);
    vec3 color = scene * (1.0 - overlay.a) + overlay.rgb;
    outColor = vec4(color, 1.0);
}
