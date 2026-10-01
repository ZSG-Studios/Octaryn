// Fullscreen-rect blit: copies a UV region of source_image into a pixel rect
// of the target, scaled by color_scale. Used for layer composite, region
// copies, and blur down/up scaling. All uniforms are float4 to keep the root
// constant layout 16-byte aligned.
Texture2D<float4> source_image;
SamplerState source_sampler;
uniform float4 dst_rect;   // x, y, w, h in target pixels
uniform float4 src_uv;     // min.xy, max.xy
uniform float4 extra;      // target_size.xy, color_scale, unused

struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    // Oversized triangle: corners (0,0) (2,0) (0,2) cover dst_rect exactly in
    // the [0,1] quadrant; the scissor rect clips the rest.
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
    return extra.z * source_image.Sample(source_sampler, v.uv);
}
