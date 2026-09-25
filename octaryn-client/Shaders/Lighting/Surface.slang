#ifndef OCTARYN_LIGHTING_SURFACE
#define OCTARYN_LIGHTING_SURFACE
#include "../Voxel/WorldPlantSurface.slang"
uint lighting_voxel(float4 value) {
    uint4 p=uint4(round(value*255));return p.x|(p.y<<8)|(p.z<<16)|(p.w<<24);
}
bool lighting_is_map(uint voxel) {return (voxel&7u)==6u;}
float2 map_sign(float2 value) {return float2(value.x>=0?1:-1,value.y>=0?1:-1);}
uint lighting_pack_map(float3 normal,float occlusion) {
    normal/=max(abs(normal.x)+abs(normal.y)+abs(normal.z),1e-8);
    float2 oct=normal.z>=0?normal.xy:(1-abs(normal.yx))*map_sign(normal.xy);
    uint2 encoded=uint2(round(saturate(oct*.5+.5)*255));
    return 6u|(uint(round(saturate(occlusion)*255))<<8)|(encoded.x<<16)|(encoded.y<<24);
}
float3 lighting_normal(uint voxel) {
    if(lighting_is_map(voxel)) {
        float2 oct=float2((voxel>>16)&255u,voxel>>24)/255.0*2-1;
        float3 normal=float3(oct,1-abs(oct.x)-abs(oct.y));
        if(normal.z<0)normal.xy=(1-abs(normal.yx))*map_sign(normal.xy);
        return normalize(normal);
    }
    if(world_plant_voxel(voxel))return world_plant_voxel_normal(voxel);
    const float3 normals[6]={float3(0,0,1),float3(0,0,-1),float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0)};
    return normals[min(voxel&7,5u)];
}
#endif
