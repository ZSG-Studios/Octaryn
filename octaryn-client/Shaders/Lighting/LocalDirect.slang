#include "ClusteredLocalResolve.slang"
RWTexture2D<float4> localLighting;
RWStructuredBuffer<uint> localCounters;
uniform uint4 localSettings; // debug: 1 light count, 2 overflow
groupshared uint groupCounters[4];
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID,uint lane:SV_GroupIndex) {
    if(lane<4)groupCounters[lane]=0;
    GroupMemoryBarrierWithGroupSync();
    if(all(id.xy<extent.xy)) {
        LocalSurface surface=local_surface(id.xy);float3 value=0;
        uint rays=0,evaluated=0;bool overflow=false;
        if(surface.valid>0) {
            value=clustered_local_lighting(surface,id.xy,rays,evaluated,overflow);
            InterlockedAdd(groupCounters[0],rays);InterlockedAdd(groupCounters[1],1);
            InterlockedAdd(groupCounters[2],evaluated);InterlockedAdd(groupCounters[3],overflow?1:0);
            if(localSettings.x==1)value=float3(log2(1+evaluated)/16.0);
            if(localSettings.x==2)value=overflow?float3(1,0,1):float3(0,.2,0);
        }
        localLighting[id.xy]=float4(max(value,0),1);
    }
    GroupMemoryBarrierWithGroupSync();
    if(lane<4&&groupCounters[lane]>0)InterlockedAdd(localCounters[lane],groupCounters[lane]);
}
