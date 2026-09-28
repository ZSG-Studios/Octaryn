#ifndef OCTARYN_BLOCK_TRANSPORT_TYPES
#define OCTARYN_BLOCK_TRANSPORT_TYPES
#include "../World/WorldPlantSurface.slang"
struct BtSurface {
    int4 key;
    float4 albedo;
    uint4 state; // geometry epoch, next sample batch, last trace frame, direct history count
    uint4 extra; // generation, resident frame, allocation frame, pin/reset bits + player revision
};
struct BtCandidate {int4 key;float4 albedo;};
static const uint BT_INVALID=0xffffffffu;
static const uint BT_TOMBSTONE=0xfffffffeu;
static const uint BT_MAX_PROBES=16;
uint bt_hash(uint value) {
    value^=value>>16;value*=0x7feb352du;value^=value>>15;
    value*=0x846ca68bu;return value^(value>>16);
}
uint bt_key_hash(int4 key) {
    return bt_hash(asuint(key.x)^bt_hash(asuint(key.y))^
        bt_hash(asuint(key.z)+0x9e3779b9u)^bt_hash(uint(key.w)+0x85ebca6bu));
}
float bt_random(inout uint state) {state=bt_hash(state);return float(state>>8)/16777216.0;}
float3 bt_normal(int direction) {
    if(world_plant_tag(uint(direction)))return world_plant_normal(uint(direction));
    const float3 normals[6]={float3(-1,0,0),float3(1,0,0),float3(0,-1,0),
        float3(0,1,0),float3(0,0,-1),float3(0,0,1)};
    return normals[clamp(direction,0,5)];
}
float3 bt_center(int4 key) {
    return float3(key.xyz)+.5+(world_plant_tag(uint(key.w))?float3(0):bt_normal(key.w)*.5);
}
bool bt_valid_key(int4 key) {return key.w>=0 && world_transport_tag(uint(key.w));}
bool bt_epoch(float value,uint epoch) {return asuint(value)==epoch;}
#endif
