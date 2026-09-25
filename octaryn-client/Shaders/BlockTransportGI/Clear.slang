#include "Types.slang"
RWStructuredBuffer<BtSurface> btSurfaces;
RWStructuredBuffer<float4> btDirect,btEnvironment;
RWStructuredBuffer<uint> btCounters;
RWStructuredBuffer<uint> btSchedule;
uniform uint4 btFrameInfo;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x<12)btCounters[id.x]=0;
    if(id.x<16)btSchedule[id.x]=0;
    if(id.x>=btFrameInfo.z)return;
    btSurfaces[id.x]=(BtSurface)0;btDirect[id.x]=0;btEnvironment[id.x]=0;
}
