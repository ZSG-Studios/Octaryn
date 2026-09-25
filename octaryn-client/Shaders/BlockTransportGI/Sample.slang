#ifndef OCTARYN_BLOCK_TRANSPORT_SAMPLE
#define OCTARYN_BLOCK_TRANSPORT_SAMPLE
#include "Types.slang"
StructuredBuffer<BtSurface> btLookupSurfaces;
StructuredBuffer<float4> btLookupIndirect,btLookupEnvironment;
uniform uint4 btLookupInfo; // active, geometry epoch, radiance epoch, capacity
bool bt_lookup_surface(uint4 packed,out uint found) {
    found=BT_INVALID;
    if(btLookupInfo.x==0 || packed.w==0 || !world_transport_tag(packed.w-1) || btLookupInfo.w==0)return false;
    int4 key=int4(asint(packed.xyz),int(packed.w)-1);
    uint first=bt_key_hash(key)&(btLookupInfo.w-1);
    for(uint attempt=0;attempt<BT_MAX_PROBES;++attempt) {
        uint row=(first+attempt)&(btLookupInfo.w-1);
        BtSurface surface=btLookupSurfaces[row];
        if(surface.state.x==0)return false;
        if(surface.state.x!=btLookupInfo.y || any(surface.key!=key))continue;
        if(surface.state.z==BT_INVALID || surface.state.w==0)return false;
        found=row;return true;
    }
    return false;
}
float3 bt_lookup_radiance(uint row,out bool complete) {
    float4 indirect=btLookupIndirect[row],environment=btLookupEnvironment[row];
    bool validIndirect=bt_epoch(indirect.w,btLookupInfo.z),validEnvironment=bt_epoch(environment.w,btLookupInfo.z);
    complete=validIndirect && validEnvironment;
    return (validIndirect?indirect.rgb:float3(0))+(validEnvironment?environment.rgb:float3(0));
}
bool sample_block_transport_gi(uint4 packed,out float3 value) {
    value=0;uint row;
    if(!bt_lookup_surface(packed,row))return false;
    bool complete;value=bt_lookup_radiance(row,complete);return true;
}
#endif
