// Mask-image composite: multiplies the source layer by the mask texture's
// alpha channel. The mask was captured with SaveLayerAsMaskImage.
Texture2D<float4> source_image;
Texture2D<float4> mask_image;
SamplerState source_sampler;
uniform float4 dst_rect;
uniform float4 src_uv;
uniform float4 extra;      // target_size.xy, unused, unused
uniform float4 mask_uv;    // min.xy, max.xy

struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; float2 mask_coord : TEXCOORD1; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    float2 corner = float2(id == 1 ? 2.0 : 0.0, id == 2 ? 2.0 : 0.0);
    float2 pixel = dst_rect.xy + corner * dst_rect.zw;
    Varying v;
    v.position = float4(pixel.x / extra.x * 2.0 - 1.0,
                        1.0 - pixel.y / extra.y * 2.0, 0, 1);
    v.uv = lerp(src_uv.xy, src_uv.zw, corner);
    v.mask_coord = lerp(mask_uv.xy, mask_uv.zw, corner);
    return v;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    float4 tex = source_image.Sample(source_sampler, v.uv);
    float mask_alpha = mask_image.Sample(source_sampler, v.mask_coord).a;
    return tex * mask_alpha;
}
