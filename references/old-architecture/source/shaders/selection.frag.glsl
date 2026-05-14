#version 450

layout(set = 3, binding = 0) uniform SelectionUniforms {
    float Brightness;
    float Alpha;
    float AtlasIndex;
    float UseTexture;
};

layout(set = 2, binding = 0) uniform sampler2DArray atlasTexture;

layout(location = 0) in vec2 vTexcoord;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = vec3(Brightness);
    float alpha = Alpha;
    if (UseTexture > 0.5)
    {
        vec4 texel = texture(atlasTexture, vec3(vTexcoord, AtlasIndex));
        color = texel.rgb * Brightness;
        alpha *= max(texel.a, 0.35);
    }
    outColor = vec4(color, alpha);
}
