#include "Reflection.slang"
#include "Hit.slang"
#include "Sampling.slang"
#include "PlayerOcclusion.slang"
RWStructuredBuffer<uint> btReceiverCounters;
void bt_record_local_ray() {
    InterlockedAdd(btReceiverCounters[1],1);
    if(playerShadowEnabled!=0)InterlockedAdd(btReceiverCounters[3],1);
}
#include "LocalSampling.slang"
#include "../Lighting/DiffuseEnvironment.slang"
struct BtDynamicReceiver {float4 position,normal;uint4 identity;};
StructuredBuffer<BtDynamicReceiver> btReceivers;
RWStructuredBuffer<BtDynamicReceiver> btPreviousReceivers;
RWStructuredBuffer<float4> btReceiverValues;
RWStructuredBuffer<float4> btReceiverDirect;
RWStructuredBuffer<uint2> btReceiverState;
uniform uint4 btReceiverWork;
uniform float4 btSun,btSky;
uniform float btReceiverShadowRange;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btReceiverWork.x)return;
    BtDynamicReceiver receiver=btReceivers[id.x],previous=btPreviousReceivers[id.x];
    float4 history=btReceiverValues[id.x];
    uint localIdentity=asuint(receiver.normal.w);
    uint2 state=btReceiverState[id.x];
    bool sameIdentity=all(receiver.identity.xy==previous.identity.xy) && localIdentity==state.x;
    btReceiverState[id.x]=uint2(localIdentity,btReceiverWork.z);
    uint batch=sameIdentity?asuint(previous.position.w):0;
    bool valid=all(isfinite(receiver.position.xyz)) && all(isfinite(receiver.normal.xyz)) &&
        dot(receiver.normal.xyz,receiver.normal.xyz)>.5;
    bool cacheValid=valid && btLookupInfo.x!=0 && receiver.identity.z==btLookupInfo.y &&
        receiver.identity.w==btLookupInfo.z && bt_inside(receiver.position.xyz);
    if(!valid) {btReceiverValues[id.x]=0;btReceiverDirect[id.x]=0;btPreviousReceivers[id.x]=receiver;return;}
    float3 normal=normalize(receiver.normal.xyz);
    float3 tangent=normalize(cross(abs(normal.y)<.99?float3(0,1,0):float3(1,0,0),normal));
    float3 bitangent=cross(normal,tangent),origin;
    cacheValid=cacheValid && bt_outward_origin(receiver.position.xyz,normal,origin);
    uint seed=bt_hash(receiver.identity.x^bt_hash(receiver.identity.y)^bt_hash(asuint(receiver.normal.w)));
    uint samples=clamp(btReceiverWork.y,1u,16u);
    float3 gathered=0;uint playerMask=0;
    for(uint sampleIndex=0;cacheValid && sampleIndex<samples;++sampleIndex) {
        float2 sample=bt_sequence(batch+sampleIndex,seed).xy;
        float radius=sqrt(sample.x),angle=sample.y*6.28318530718;
        float3 direction=tangent*(radius*cos(angle))+bitangent*(radius*sin(angle))+normal*sqrt(1-sample.x);
        float skyDistance=bt_sky_distance(origin,direction);
        bool sky=skyDistance>0 && skyDistance<=raySettings.y;
        float range=sky?max(32.0,skyDistance):32.0;
        InterlockedAdd(btReceiverCounters[0],1);
        WorldRayHit hit=world_trace_ray(origin,direction,range,false,float3(1e30),false,false,true,0);
        if(playerShadowEnabled!=0) {
            InterlockedAdd(btReceiverCounters[3],1);
            if(bt_player_occluded(origin,direction,min(range,hit.distance),0)) {playerMask|=1u<<sampleIndex;continue;}
        }
        if(!hit.hit) {if(sky)gathered+=diffuse_environment(direction,btSun.xyz,btSky);continue;}
        if(hit.distance>32 || !bt_inside(hit.position))continue;
        int4 key;uint row;float3 incident;
        if(!bt_hit_key(hit,key) || !bt_lookup_surface(hit.surfaceKey,row) ||
            !sample_block_transport_reflected_incident(hit.surfaceKey,hit.backFace,incident))continue;
        gathered+=btLookupSurfaces[row].albedo.rgb*incident;
    }
    gathered/=float(samples);
    bool actorChanged=state.y!=btReceiverWork.z;
    bool actorAffected=actorChanged && (playerMask!=0 || asuint(previous.normal.w)!=0);
    bool reuse=cacheValid && sameIdentity && !actorAffected &&
        all(receiver.identity==previous.identity) && all(receiver.position.xyz==previous.position.xyz) &&
        all(receiver.normal.xyz==previous.normal.xyz) && all(isfinite(history)) && history.w>=1;
    float count=reuse?min(history.w+1,16):1;
    if(reuse)gathered=lerp(history.rgb,gathered,1/count);
    btReceiverValues[id.x]=cacheValid && all(isfinite(gathered))?float4(max(gathered,0),count):float4(0);
    uint random=bt_hash(seed^bt_hash(batch));
    float3 local=bt_local_direct(receiver.position.xyz,normal,random,false);
    float solar=0;
    if(btSun.w>0 && dot(normal,btSun.xyz)>0) {
        solar=1;
        if(raySettings.x!=0) {
            InterlockedAdd(btReceiverCounters[2],1);
            if(playerShadowEnabled!=0)InterlockedAdd(btReceiverCounters[3],1);
            // Match the primary plane-receiver ray, range and distance fade.
            float3 light=normalize(btSun.xyz);
            float3 directOrigin=receiver.position.xyz+normal*.002+light*.0005;
            WorldRayHit hit=world_trace_ray(directOrigin,light,raySettings.y,true);
            float fade=btReceiverShadowRange>0?btReceiverShadowRange:raySettings.y;
            solar=hit.hit?saturate((hit.distance/fade-.875)*8):1;
        }
    }
    btReceiverDirect[id.x]=float4(local,solar);
    receiver.position.w=asfloat(batch+samples);receiver.normal.w=asfloat(playerMask);btPreviousReceivers[id.x]=receiver;
}
