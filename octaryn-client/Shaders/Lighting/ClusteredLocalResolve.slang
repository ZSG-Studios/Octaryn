#ifndef OCTARYN_CLUSTERED_LOCAL_RESOLVE
#define OCTARYN_CLUSTERED_LOCAL_RESOLVE
#include "LocalLighting.slang"
#ifdef OCTARYN_LOCAL_DIRECT_RT
#include "LocalVisibility.slang"
#else
#include "../Shadows/LocalSample.slang"
#endif
StructuredBuffer<uint> localTileCounts,localTileLights;
uniform uint4 localTileSettings;
float3 clustered_local_lighting(LocalSurface surface,uint2 pixel,out uint rays,out uint evaluated,out bool overflow) {
    uint2 tile=pixel/localTileSettings.w;
    uint index=tile.y*localTileSettings.x+tile.x,count=localTileCounts[index];
    overflow=count>localTileSettings.z;
    // Saturated lists evaluate the complete source list, never a random subset.
    uint length=overflow?extent.w:count;
    rays=0;evaluated=length;float3 value=0;
    const float2 samples[4]={float2(.25,.25),float2(.75,.25),float2(.25,.75),float2(.75,.75)};
    for(uint i=0;i<length;++i) {
        uint light=overflow?i:localTileLights[index*localTileSettings.z+i];
        uint sampleCount=uint(localLights[light].axisVType.w)==2?4:1;
        for(uint j=0;j<sampleCount;++j) {
            float3 L;float distance;
            float3 incident=local_contribution(surface,light,sampleCount==4?samples[j]:float2(.5),L,distance);
            if(!any(incident>0))continue;
#ifdef OCTARYN_LOCAL_DIRECT_RT
            float visibility=world_local_light_visibility(surface.position,surface.normal,light,L,distance);
            rays+=distance<=raySettings.y?1:0;
#else
            float visibility=local_shadow_visibility(light,surface.position,surface.normal);
#endif
            value+=incident*(visibility/sampleCount);
        }
    }
    return value;
}
#endif
