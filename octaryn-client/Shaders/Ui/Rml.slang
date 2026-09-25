// Rml::Vertex is packed as position, premultiplied RGBA, texture coordinates.
ByteAddressBuffer vertices;
Texture2D<float4> image;
SamplerState imageSampler;
uniform float4 transformRow0;
uniform float4 transformRow1;
uniform float4 transformRow2;
uniform float4 transformRow3;
uniform float4 translationScale;
struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    uint offset = id * 20;
    float2 position = asfloat(vertices.Load2(offset)) + translationScale.xy;
    uint color = vertices.Load(offset + 8);
    float4 local = float4(position, 0, 1);
    float4 transformed = float4(dot(transformRow0, local), dot(transformRow1, local),
                                dot(transformRow2, local), dot(transformRow3, local));
    Varying result;
    result.position = float4(transformed.x * translationScale.z - transformed.w,
                             transformed.w - transformed.y * translationScale.w, 0, transformed.w);
    result.uv = asfloat(vertices.Load2(offset + 12));
    result.color = float4(color & 255, (color >> 8) & 255,
                         (color >> 16) & 255, color >> 24) / 255.0;
    return result;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    return v.color * image.Sample(imageSampler, v.uv);
}
