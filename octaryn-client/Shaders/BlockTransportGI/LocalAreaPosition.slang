#ifndef OCTARYN_BLOCK_TRANSPORT_LOCAL_AREA_POSITION
#define OCTARYN_BLOCK_TRANSPORT_LOCAL_AREA_POSITION
#include "Sampling.slang"
bool bt_local_area_position(BtSurface surface,uint batch,uint candidate,inout uint random,out float3 position) {
    // Eight contiguous points preserve each dyadic block; the high batch bits shift on carry.
    uint index=((batch<<3)|(candidate&7u))^(bt_hash(batch>>29)&0xfffffff8u);
    return bt_surface_position(surface,index,random,position);
}
#endif
