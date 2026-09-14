#ifndef OCTARYN_CLUSTERED_LOCAL_RESOLVE
#define OCTARYN_CLUSTERED_LOCAL_RESOLVE
#include "../Shadows/LocalSample.slang"
StructuredBuffer<uint> localTileCounts,localTileLights;
uniform uint4 localTileSettings;
float3 clustered_local_lighting(LocalSurface surface,uint2 pixel,float3 overflowEstimate) {
    uint2 tile=pixel/localTileSettings.w;
    uint index=tile.y*localTileSettings.x+tile.x,count=localTileCounts[index];
    // Full-support RIS on saturated tiles preserves lights beyond the fixed list capacity.
    if(count>localTileSettings.z)return overflowEstimate;
    uint rng=restir_hash(pixel.y*extent.x+pixel.x+restir_hash(extent.z+671));
    float3 value=0;
    for(uint i=0;i<count;++i) {
        uint light=localTileLights[index*localTileSettings.z+i];
        float2 sample=float2(restir_random(rng),restir_random(rng));
        float3 L;float distance;
        value+=local_contribution(surface,light,sample,L,distance)*local_shadow_visibility(light,surface.position,surface.normal);
    }
    return value;
}
#endif
