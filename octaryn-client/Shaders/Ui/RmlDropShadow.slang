// Drop-shadow filter: draws the source layer's alpha silhouette, offset by
// the shadow displacement, tinted with the premultiplied shadow color. The
// unmodified source is composited over this in a second pass.
Texture2D<float4> source_image;
SamplerState source_sampler;
uniform float4 dst_rect;    // x, y, w, h in target pixels
uniform float4 src_uv;      // min.xy, max.xy (unshifted source region)
uniform float4 extra;       // target_size.xy, shadow offset in UV units
uniform float4 region;      // uv bounds of the source content: min.xy, max.xy
uniform float4 shadow_color;

struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    float2 corner = float2(id == 1 ? 2.0 : 0.0, id == 2 ? 2.0 : 0.0);
    float2 pixel = dst_rect.xy + corner * dst_rect.zw;
    Varying v;
    v.position = float4(pixel.x / extra.x * 2.0 - 1.0,
                        1.0 - pixel.y / extra.y * 2.0, 0, 1);
    v.uv = lerp(src_uv.xy, src_uv.zw, corner) - extra.zw;
    return v;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    float2 inside = step(region.xy, v.uv) * step(v.uv, region.zw);
    float alpha = source_image.Sample(source_sampler, v.uv).a * inside.x * inside.y;
    return alpha * shadow_color;
}
