#include "Cache.slang"
RWStructuredBuffer<float4> btDirect,btEnvironment;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btFrameInfo.z || btCounters[11]==0)return;
    BtSurface surface=btSurfaces[id.x];
    if(surface.state.x!=btFrameInfo.x || (surface.extra.w&1)!=0)return;
    uint residentAge=btFrameInfo.y-surface.extra.y;
    uint allocationAge=btFrameInfo.y-surface.extra.z;
    if(residentAge<=64 || allocationAge<=64)return;
    uint previous;
    InterlockedCompareExchange(btSurfaces[id.x].state.x,btFrameInfo.x,BT_INVALID,previous);
    if(previous!=btFrameInfo.x)return;
    btSurfaces[id.x].state.yzw=uint3(0,BT_INVALID,0);
    btDirect[id.x]=0;btEnvironment[id.x]=0;
    DeviceMemoryBarrier();
    InterlockedExchange(btSurfaces[id.x].state.x,BT_TOMBSTONE);
    InterlockedAdd(btCounters[8],0xffffffffu);
    if(surface.state.z!=BT_INVALID)InterlockedAdd(btCounters[9],0xffffffffu);
}
