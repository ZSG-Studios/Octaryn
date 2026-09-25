#ifndef OCTARYN_BLOCK_TRANSPORT_CUBE_MASK
#define OCTARYN_BLOCK_TRANSPORT_CUBE_MASK
#include "PlantMask.slang"
StructuredBuffer<float4> btCubeMaskLayers;
bool bt_cube_mask_ready(uint layer) {
    if(layer>=29)return false;
    uint count=asuint(btCubeMaskLayers[layer].w);return count>0 && count<=1024;
}
bool bt_cube_mask_layer(float domain,out uint layer) {
    layer=0;
    if(!isfinite(domain) || domain>-1 || domain<-29 || floor(domain)!=domain)return false;
    layer=uint(-domain)-1;return bt_cube_mask_ready(layer);
}
float3 bt_cube_mask_offset(int face,float2 uv) {
    if(face==0)return float3(0,1-uv.y,uv.x);
    if(face==1)return float3(1,1-uv.y,1-uv.x);
    if(face==2)return float3(uv.x,0,uv.y);
    if(face==3)return float3(uv.x,1,1-uv.y);
    if(face==4)return float3(1-uv.x,1-uv.y,0);
    return float3(uv.x,1-uv.y,1);
}
bool bt_cube_owns_texel(int4 key,float3 position,uint2 texel) {
    float3 p=position-float3(key.xyz);
    if(any(p<0) || any(p>1))return false;
    float2 uv=key.w==0?float2(p.z,1-p.y):key.w==1?float2(1-p.z,1-p.y):
        key.w==2?float2(p.x,p.z):key.w==3?float2(p.x,1-p.z):
        key.w==4?float2(1-p.x,1-p.y):float2(p.x,1-p.y);
    return all(uint2(floor(frac(uv)*32))==texel);
}
bool bt_cube_mask_position(int4 key,float domain,float2 sample,out float3 position) {
    position=0;uint layer;float3 low,high;
    if(!bt_cell_bounds(key.xyz,low,high))return false;
    if(key.w<0 || key.w>=6 || !bt_cube_mask_layer(domain,layer))return false;
    uint count=asuint(btCubeMaskLayers[layer].w);
    // Cubes tile all 32 texels at equal area; sprite alias weights are different.
    float scaled=sample.x*float(count);uint selected=min(uint(scaled),count-1);
    uint packed=btPlantMaskCells[layer*1024+selected].x;
    uint2 texel=uint2(packed&31,(packed>>5)&31);
    float2 within=clamp(float2(scaled-float(selected),sample.y),1.0/16777216.0,1-1.0/16777216.0);
    float2 uv=(float2(texel)+within)/32;
    position=float3(key.xyz)+bt_cube_mask_offset(key.w,uv);
    if(bt_cube_interior(key,position) && bt_cube_owns_texel(key,position,texel))return true;
    // A fixed center retry handles world-float rounding without rejection loops.
    position=float3(key.xyz)+bt_cube_mask_offset(key.w,(float2(texel)+.5)/32);
    return bt_cube_interior(key,position) && bt_cube_owns_texel(key,position,texel);
}
#endif
