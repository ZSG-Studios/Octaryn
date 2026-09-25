#ifndef OCTARYN_BLOCK_TRANSPORT_CACHE
#define OCTARYN_BLOCK_TRANSPORT_CACHE
#include "Types.slang"
#ifdef BT_CACHE_READONLY
StructuredBuffer<BtSurface> btSurfaces;
#else
globallycoherent RWStructuredBuffer<BtSurface> btSurfaces;
RWStructuredBuffer<uint> btCounters;
#endif
uniform uint4 btFrameInfo; // epoch, frame, capacity, links per patch
uint bt_find(int4 key) {
    uint first=bt_key_hash(key)&(btFrameInfo.z-1);
    for(uint attempt=0;attempt<BT_MAX_PROBES;++attempt) {
        uint index=(first+attempt)&(btFrameInfo.z-1);
        uint epoch=btSurfaces[index].state.x;
        if(epoch==btFrameInfo.x && all(btSurfaces[index].key==key))return index;
        if(epoch==0)return BT_INVALID;
    }
    return BT_INVALID;
}
#ifndef BT_CACHE_READONLY
uint bt_admit(int4 key,float4 albedo,bool resident=false,bool pinned=false,uint allocationLimit=BT_INVALID) {
    if(!bt_valid_key(key))return BT_INVALID;
    InterlockedAdd(btCounters[0],1);
    uint first=bt_key_hash(key)&(btFrameInfo.z-1);
    uint reusable=BT_INVALID,reusableState=0;
    bool pending=false;
    // Search past tombstones before claiming, or an existing key could duplicate.
    for(uint attempt=0;attempt<BT_MAX_PROBES;++attempt) {
        uint index=(first+attempt)&(btFrameInfo.z-1),epoch;
        InterlockedCompareExchange(btSurfaces[index].state.x,BT_INVALID,BT_INVALID,epoch);
        if(epoch==btFrameInfo.x) {
            DeviceMemoryBarrier();
            if(all(btSurfaces[index].key==key)) {
                InterlockedExchange(btSurfaces[index].extra.y,btFrameInfo.y);
                if(pinned)InterlockedOr(btSurfaces[index].extra.w,1);
                return index;
            }
        } else if(epoch==BT_INVALID)pending=true;
        else if(epoch==0 || epoch==BT_TOMBSTONE) {
            DeviceMemoryBarrier();
            // Retire a generation at its maximum until a geometry-epoch clear.
            if(reusable==BT_INVALID && btSurfaces[index].extra.x!=BT_INVALID) {
                reusable=index;reusableState=epoch;
            }
        }
    }
    // A pending claim may be this same key. Never claim a second location.
    if(pending) {
        InterlockedAdd(btCounters[1],1);
        if(resident)InterlockedAdd(btCounters[11],1);
        return BT_INVALID;
    }
    if(reusable==BT_INVALID) {
        InterlockedAdd(btCounters[1],1);
        if(resident)InterlockedAdd(btCounters[11],1);
        return BT_INVALID;
    }
    uint previous;
    InterlockedCompareExchange(btSurfaces[reusable].state.x,reusableState,BT_INVALID,previous);
    if(previous!=reusableState) {
        InterlockedAdd(btCounters[1],1);
        if(resident)InterlockedAdd(btCounters[11],1);
        return BT_INVALID;
    }
    uint generation=btSurfaces[reusable].extra.x+1;
    // Reserve occupancy atomically before publication, including concurrent contributors.
    uint occupied;InterlockedAdd(btCounters[8],1,occupied);
    if(occupied>=min(allocationLimit,btFrameInfo.z)) {
        InterlockedAdd(btCounters[8],0xffffffffu);
        InterlockedExchange(btSurfaces[reusable].state.x,reusableState);
        InterlockedAdd(btCounters[1],1);
        if(resident)InterlockedAdd(btCounters[11],1);
        return BT_INVALID;
    }
    btSurfaces[reusable].key=key;btSurfaces[reusable].albedo=albedo;
    btSurfaces[reusable].state.yzw=uint3(0,BT_INVALID,0);
    btSurfaces[reusable].extra=uint4(generation,btFrameInfo.y,btFrameInfo.y,pinned?1:0);
    DeviceMemoryBarrier();
    InterlockedExchange(btSurfaces[reusable].state.x,btFrameInfo.x);
    return reusable;
}
#endif
#endif
