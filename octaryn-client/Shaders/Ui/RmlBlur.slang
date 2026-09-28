// Separable gaussian blur with three-sigma support in target pixels.
// Samples outside the active region contribute transparent black.
Texture2D<float4> source_image;
SamplerState source_sampler;
uniform float4 dst_rect;    // x, y, w, h in target pixels
uniform float4 src_uv;      // min.xy, max.xy
uniform float4 extra;       // target_size.xy, unused, unused
uniform float4 texel;       // direction * 1/dim, sigma, unused
uniform float4 region;      // uv bounds of the blurred region: min.xy, max.xy

struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    float2 corner = float2(id == 1 ? 2.0 : 0.0, id == 2 ? 2.0 : 0.0);
    float2 pixel = dst_rect.xy + corner * dst_rect.zw;
    Varying v;
    v.position = float4(pixel.x / extra.x * 2.0 - 1.0,
                        1.0 - pixel.y / extra.y * 2.0, 0, 1);
    v.uv = lerp(src_uv.xy, src_uv.zw, corner);
    return v;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    if (texel.z < 0.1) return source_image.Sample(source_sampler, v.uv);
    int radius = int(ceil(3.0 * texel.z));
    float4 color = 0.0;
    float normalization = 0.0;
    for (int tap = -radius; tap <= radius; ++tap) {
        float weight = exp(-0.5 * (float(tap) / texel.z) * (float(tap) / texel.z));
        float2 uv = v.uv - float(tap) * texel.xy;
        float2 inside = step(region.xy, uv) * step(uv, region.zw);
        color += source_image.Sample(source_sampler, uv) * inside.x * inside.y * weight;
        normalization += weight;
    }
    return color / normalization;
}
