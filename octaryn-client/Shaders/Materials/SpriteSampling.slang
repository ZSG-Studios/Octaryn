#ifndef OCTARYN_SPRITESAMPLING_SLANG
#define OCTARYN_SPRITESAMPLING_SLANG
// Declared after the retained mesh resources to preserve their binding indices.
SamplerState atlasSprite;
float4 sample_sprite_albedo_grad(float2 uv,uint voxel,float2 dx,float2 dy) {
    float2 texel=.5/atlas_dimensions(atlasAlbedo);
    float2 scale=1-2*texel;
    float2 coordinate=lerp(texel,1-texel,saturate(uv));
    // Coarse mip footprints clamp at the layer edge instead of wrapping to the
    // opposite side of a flower or torch. UI previews keep their own LOD0 path.
    return sample_pixel_albedo(atlasAlbedo,atlasSprite,coordinate,voxel,dx*scale,dy*scale);
}

#endif
