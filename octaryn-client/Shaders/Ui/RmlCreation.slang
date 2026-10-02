// "Creation" shader decorator (Danilo Guanabara, shadertoy XsXXDn port):
// animated procedural pattern; uv spans the element 0..1.
ByteAddressBuffer vertices;
uniform float4 transformRow0;
uniform float4 transformRow1;
uniform float4 transformRow2;
uniform float4 transformRow3;
uniform float4 translationScale; // translate.xy, 2/dims.xy
uniform float4 creation;         // value (time), dimensions.xy, unused
uniform float4 vertexTint;

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
                         (color >> 16) & 255, color >> 24) / 255.0 * vertexTint;
    return result;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    float t = creation.x;
    float3 c = 0.0;
    float l = 1.0;
    for (int i = 0; i < 3; ++i) {
        float2 p = v.uv;
        float2 uv = p;
        p -= 0.5;
        p.x *= creation.y / max(creation.z, 1.0);
        float z = t + float(i) * 0.07;
        l = length(p);
        uv += p / l * (sin(z) + 1.0) * abs(sin(l * 9.0 - z - z));
        c[i] = 0.01 / length(fmod(uv, 1.0) - 0.5);
    }
    return float4(c / l, v.color.a);
}
