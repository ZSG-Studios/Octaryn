#ifndef OCTARYN_BLOCK_TRANSPORT_PLANT_MASK
#define OCTARYN_BLOCK_TRANSPORT_PLANT_MASK
#include "Types.slang"
#include "SurfaceOrigin.slang"
StructuredBuffer<uint4> btPlantMaskCells;
StructuredBuffer<float4> btPlantMaskLayers;
uint bt_plant_mask_count(uint tag) {return asuint(btPlantMaskLayers[tag>>4].w);}
bool bt_plant_mask_ready(int4 key) {
    if(!world_plant_tag(uint(key.w)))return true;
    uint count=bt_plant_mask_count(uint(key.w));return count>0 && count<=1024;
}
float bt_plant_open_random(inout uint random) {
    return clamp(bt_random(random),1.0/16777216.0,1-1.0/16777216.0);
}
bool bt_plant_owns_texel(int4 key,float3 position,uint2 texel) {
    float3 local=position-float3(key.xyz);
    if(any(local<0) || any(local>1))return false;
    float3 uv=float3(1-local.x,1-local.y,
        world_plant_plane(uint(key.w))==0?1-local.z:local.z);
    uint3 sampled=uint3(floor(.5+31*uv));
    return sampled.x==texel.x && sampled.y==texel.y && sampled.z==texel.x;
}
bool bt_plant_position(int4 key,inout uint random,out float3 position) {
    position=0;float3 cellLow,cellHigh;
    if(!bt_cell_bounds(key.xyz,cellLow,cellHigh))return false;
    uint tag=uint(key.w),count=bt_plant_mask_count(tag);
    if(count==0 || count>1024)return false;
    uint base=(tag>>4)*1024;
    uint selected=min(uint(bt_random(random)*float(count)),count-1);
    uint4 entry=btPlantMaskCells[base+selected];
    if(bt_random(random)>=asfloat(entry.z)) {
        if(entry.y>=count)return false;
        entry=btPlantMaskCells[base+entry.y];
    }
    uint2 texel=uint2(entry.x&31,(entry.x>>5)&31);
    float2 low=max(float2(0),(float2(texel)-.5)/31.0);
    float2 high=min(float2(1),(float2(texel)+.5)/31.0);
    float2 uv=lerp(low,high,float2(bt_plant_open_random(random),bt_plant_open_random(random)));
    position=float3(key.xyz)+world_plant_offset(tag,uv);
    // World-float rounding can cross a texel boundary. One center retry is bounded.
    float3 origin;
    if(bt_plant_owns_texel(key,position,texel) && bt_outward_origin(position,bt_normal(key.w),origin) &&
        all(origin>cellLow) && all(origin<cellHigh))return true;
    position=float3(key.xyz)+world_plant_offset(tag,(low+high)*.5);
    return bt_plant_owns_texel(key,position,texel) && bt_outward_origin(position,bt_normal(key.w),origin) &&
        all(origin>cellLow) && all(origin<cellHigh);
}
#endif
