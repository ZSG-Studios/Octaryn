#ifndef OCTARYN_WORLD_SURFACE_KEY
#define OCTARYN_WORLD_SURFACE_KEY
#include "WorldPlantSurface.slang"
bool world_surface_transport_supported(uint flags) {
    // Catalog opaque is bit 4; bit 0 is occlusion and excludes cutout leaves.
    return (flags&16u)!=0 && (flags&(2u|4u|(1u<<30)))==0;
}
uint4 world_surface_address(uint4 face,float3 local,out float2 uv) {
    uv=-1;
    uint direction=(face.w>>16)&15;
    if(direction>=6 || !all(isfinite(local)))return uint4(0);
    int u=int(((face.w>>20)&31)+1),v=int(((face.w>>25)&31)+1);
    int3 extent=direction<2?int3(1,v,u):direction<4?int3(u,1,v):int3(u,v,1);
    int3 offset=int3(clamp(floor(local),float3(0),float3(extent-1)));
    int3 anchor=asint(face.xyz);
    // Local offsets are nonnegative, so only the upper signed limit can overflow.
    if(any(anchor>int3(2147483647)-offset))return uint4(0);
    float3 within=saturate(local-float3(offset));
    uv=direction<2?within.zy:direction<4?within.xz:within.xy;
    return uint4(asuint(anchor+offset),direction+1);
}
uint4 world_surface_key(uint4 face,float3 local) {
    float2 uv;return world_surface_address(face,local,uv);
}
#endif
