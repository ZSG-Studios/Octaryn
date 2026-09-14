#ifndef OCTARYN_MATERIALSAMPLING_SLANG
#define OCTARYN_MATERIALSAMPLING_SLANG
float3 octaryn_default_albedo()
{
    return float3(1.0, 1.0, 1.0);
}

#include "MaterialTypes.slang"
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
Texture2DArray<float4> atlasAlbedo;
Texture2DArray<float4> atlasNormal;
Texture2DArray<float4> atlasSpecular;
SamplerState atlasCutout;
SamplerState atlasLinear;
static const uint kDirectionNorth=0, kDirectionSouth=1, kDirectionEast=2;
static const uint kDirectionWest=3, kDirectionUp=4, kDirectionDown=5;
uint get_direction(uint voxel) { return voxel & 7; }
uint get_index(uint voxel) { return (voxel >> 3) & 31; }
bool is_sprite_voxel(uint voxel) { return (voxel & 256) != 0; }
float3 get_normal(uint voxel) {
    const float3 normals[6]={float3(0,0,1),float3(0,0,-1),float3(1,0,0),
        float3(-1,0,0),float3(0,1,0),float3(0,-1,0)};
    return normals[get_direction(voxel)];
}
float2 atlas_dimensions(Texture2DArray<float4> atlas) {
    uint width,height,layers; atlas.GetDimensions(width,height,layers);
    return float2(width,height);
}
float4 sample_face_tiled_atlas_grad(Texture2DArray<float4> atlas,float2 uv,uint voxel,float2 dx,float2 dy) {
    // Encoded LabPBR categories and emission sentinels must not be interpolated.
    return atlas.SampleGrad(atlasCutout,float3(uv,float(get_index(voxel))),dx,dy);
}
float4 sample_pixel_albedo(Texture2DArray<float4> atlas,SamplerState filtered,
                          float2 uv,uint voxel,float2 dx,float2 dy) {
    float2 size=atlas_dimensions(atlas);
    float footprint=max(length(dx*size),length(dy*size));
    float3 coordinate=float3(uv,float(get_index(voxel)));
    // Crisp magnification; no negative LOD bias when the footprint minifies.
    if(footprint<=.5) return atlas.SampleLevel(atlasCutout,coordinate,0);
    float4 smooth=atlas.SampleGrad(filtered,coordinate,dx,dy);
    if(footprint>=1) return smooth;
    return lerp(atlas.SampleLevel(atlasCutout,coordinate,0),smooth,smoothstep(.5,1,footprint));
}
float4 sample_face_albedo_grad(float2 uv,uint voxel,float2 dx,float2 dy) {
    return sample_pixel_albedo(atlasAlbedo,atlasLinear,uv,voxel,dx,dy);
}
float4 sample_sprite_atlas(Texture2DArray<float4> atlas,float2 uv,uint voxel) {
    float2 texel=0.5/atlas_dimensions(atlas);
    return atlas.SampleLevel(atlasCutout,float3(lerp(texel,1-texel,saturate(uv)),float(get_index(voxel))),0);
}

#endif
