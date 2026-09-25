#include "Types.slang"
StructuredBuffer<BtSurface> btSurfaces;
StructuredBuffer<float4> btDirect,btEnvironment,btIndirect;
RWStructuredBuffer<uint4> btWorkRows,btWorkBlocks;
RWStructuredBuffer<uint> btSchedule;
uniform uint4 btFrameInfo,btWork;
groupshared uint counts[256],ready[256],pinned[256];
// A fixed scan of the hash pool feeds only live rows into the bounded ray work.
[shader("compute")]
[numthreads(256,1,1)]
void count_main(uint3 group:SV_GroupID,uint3 lane:SV_GroupThreadID,uint3 id:SV_DispatchThreadID) {
    uint row=(btSchedule[0]+id.x)%btFrameInfo.z;
    BtSurface surface=btSurfaces[row];
    bool active=id.x<btFrameInfo.z && surface.state.x==btFrameInfo.x;
    counts[lane.x]=active?1:0;
    pinned[lane.x]=active && (surface.extra.w&1)!=0?1:0;
    ready[lane.x]=active && surface.state.z!=BT_INVALID && surface.state.w>0 &&
        bt_epoch(btDirect[row].w,btWork.z) && bt_epoch(btEnvironment[row].w,btWork.z) &&
        bt_epoch(btIndirect[row].w,btWork.z)?1:0;
    GroupMemoryBarrierWithGroupSync();
    for(uint stride=128;stride>0;stride>>=1) {
        if(lane.x<stride) {
            counts[lane.x]+=counts[lane.x+stride];ready[lane.x]+=ready[lane.x+stride];
            pinned[lane.x]+=pinned[lane.x+stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if(lane.x==0)btWorkBlocks[group.x]=uint4(counts[0],0,ready[0],pinned[0]);
}
[shader("compute")]
[numthreads(1,1,1)]
void prefix_main(uint3 id:SV_DispatchThreadID) {
    uint total=0,fresh=0,resident=0,groups=(btFrameInfo.z+255)/256;
    for(uint group=0;group<groups;++group) {
        uint4 block=btWorkBlocks[group];block.y=total;btWorkBlocks[group]=block;
        total+=block.x;fresh+=block.z;resident+=block.w;
    }
    btSchedule[1]=btSchedule[0];btSchedule[2]=min(total,btWork.y);
    btSchedule[3]=total;btSchedule[4]=fresh;
    btSchedule[5]=btFrameInfo.x;btSchedule[6]=btWork.z;btSchedule[7]=btFrameInfo.y;
    btSchedule[9]=0;btSchedule[10]=0;
    btSchedule[11]=resident;
}
[shader("compute")]
[numthreads(256,1,1)]
void select_main(uint3 group:SV_GroupID,uint3 lane:SV_GroupThreadID,uint3 id:SV_DispatchThreadID) {
    uint start=btSchedule[1],row=(start+id.x)%btFrameInfo.z;
    BtSurface surface=btSurfaces[row];
    bool active=id.x<btFrameInfo.z && surface.state.x==btFrameInfo.x;
    counts[lane.x]=active?1:0;GroupMemoryBarrierWithGroupSync();
    for(uint stride=1;stride<256;stride<<=1) {
        uint previous=lane.x>=stride?counts[lane.x-stride]:0;
        GroupMemoryBarrierWithGroupSync();counts[lane.x]+=previous;GroupMemoryBarrierWithGroupSync();
    }
    if(!active)return;
    uint rank=btWorkBlocks[group.x].y+counts[lane.x]-1;
    if(rank>=btSchedule[2])return;
    btWorkRows[rank]=uint4(row,surface.extra.x,btFrameInfo.x,surface.extra.w>>2);
    if(rank+1==btSchedule[2]) {
        btSchedule[0]=(row+1)%btFrameInfo.z;
        if(start+id.x+1>=btFrameInfo.z)btSchedule[8]+=1;
    }
}
