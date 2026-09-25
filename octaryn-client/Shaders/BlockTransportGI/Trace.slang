#include "Cache.slang"
#include "Sampling.slang"
#include "Hit.slang"
#include "Reflectance.slang"
#define BT_WORK_WRITABLE_SCHEDULE
#include "Work.slang"
#include "PlayerOcclusion.slang"
void bt_record_player_history_query() {InterlockedAdd(btSchedule[10],1);}
#include "ActorHistory.slang"
RWStructuredBuffer<uint4> btLinks;
RWStructuredBuffer<BtCandidate> btCandidates;
uniform uint btPlayerRevision;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint count=btWork.y*btFrameInfo.w;if(id.x>=count)return;
    btCandidates[id.x]=(BtCandidate)0;btCandidates[id.x].key.w=-1;
    uint row;if(!bt_work_row(id.x/btFrameInfo.w,row))return;
    uint slot=id.x%btFrameInfo.w,address=row*btFrameInfo.w+slot;
    uint4 previous=btLinks[address];
    BtSurface surface=btSurfaces[row];
    bool playerChanged=btWorkRows[id.x/btFrameInfo.w].w!=btPlayerRevision;
    btLinks[address]=uint4(BT_INVALID,0,0,0);
    if(surface.state.x!=btFrameInfo.x)return;
    if(playerChanged && bt_previous_player_changed(surface,slot,previous))
        InterlockedOr(btSurfaces[row].extra.w,2);
    if(slot==0) {
        InterlockedAnd(btSurfaces[row].extra.w,3);
        InterlockedOr(btSurfaces[row].extra.w,btPlayerRevision<<2);
        if(btSurfaces[row].state.z==BT_INVALID)InterlockedAdd(btCounters[9],1);
        InterlockedAdd(btCounters[10],1);
        btSurfaces[row].state.z=btFrameInfo.y;
    }
    float3 origin,direction;
    if(!bt_surface_sample(btSurfaces[row],slot,btSurfaces[row].state.y,origin,direction) || !bt_inside(origin)) {
        InterlockedAdd(btCounters[4],1);return;
    }
    float skyDistance=bt_sky_distance(origin,direction);
    bool skyReachable=skyDistance>0 && skyDistance<=raySettings.y;
    float range=skyReachable?max(32.0,skyDistance):32.0;
    InterlockedAdd(btCounters[6],1);
    WorldRayHit hit=world_trace_ray(origin,direction,range,false,float3(1e30),false,true,true,0);
    bool playerBlocked=false;
    if(playerShadowEnabled!=0) {
        InterlockedAdd(btSchedule[9],1);
        playerBlocked=bt_player_occluded(origin,direction,min(range,hit.distance),0);
    }
    if(playerBlocked) {
        btLinks[address]=uint4(asuint(min(range,hit.distance)),btFrameInfo.x,0,4);
        InterlockedAdd(btCounters[4],1);return;
    }
    if(!hit.hit) {
        if(skyReachable) {btLinks[address].w=2;InterlockedAdd(btCounters[3],1);}
        else InterlockedAdd(btCounters[4],1);
        return;
    }
    if(hit.distance>32 || !bt_inside(hit.position)) {InterlockedAdd(btCounters[4],1);return;}
    int4 target;float4 targetAlbedo;
    if(!bt_hit_key(hit,target) || !bt_surface_material(target,hit.voxel,hit.flags,targetAlbedo)) {InterlockedAdd(btCounters[5],1);return;}
    InterlockedAdd(btCounters[2],1);
    btCandidates[id.x].key=target;
    btCandidates[id.x].albedo=targetAlbedo;
    btLinks[address]=uint4(id.x,0,0,3);
}
