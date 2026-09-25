#ifndef OCTARYN_BLOCK_TRANSPORT_REFLECTANCE
#define OCTARYN_BLOCK_TRANSPORT_REFLECTANCE
#include "../Materials/MaterialSampling.slang"
#include "PlantMask.slang"
#include "CubeMask.slang"
float3 bt_reflectance(uint voxel) {
    float2 size=atlas_dimensions(atlasAlbedo);
    float mip=floor(log2(max(size.x,size.y)));
    float3 coordinate=float3(.5,.5,get_index(voxel));
    // The atlas builds its albedo mips in linear space. The last mip represents
    // the whole opaque block face rather than the first visible texture pixel.
    float3 albedo=atlasAlbedo.SampleLevel(atlasLinear,coordinate,mip).rgb;
    float metallic=atlasSpecular.SampleLevel(atlasCutout,coordinate,mip).g>=(230.0/255.0)?1:0;
    return saturate(albedo)*(1-metallic)*.96;
}
// Domain: 1 is a solid cube, 0 is a plant, and -(layer+1) is a masked cube.
bool bt_surface_material(int4 key,uint voxel,uint flags,out float4 value) {
    value=0;
    if(world_plant_tag(uint(key.w))) {
        if(!bt_plant_mask_ready(key))return false;
        value=float4(btPlantMaskLayers[uint(key.w)>>4].rgb,0);return true;
    }
    if((flags&1)!=0) {value=float4(bt_reflectance(voxel),1);return true;}
    uint layer=get_index(voxel);
    if(!bt_cube_mask_ready(layer))return false;
    value=float4(btCubeMaskLayers[layer].rgb,-float(layer+1));return true;
}
#endif
