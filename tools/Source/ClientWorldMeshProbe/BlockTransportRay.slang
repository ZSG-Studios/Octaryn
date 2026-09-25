#include "../../../octaryn-client/Shaders/BlockTransportGI/Sampling.slang"
RWStructuredBuffer<float4> probeSamples;
uniform int4 probeKey;
uniform uint4 probeSampleInfo;
[shader("compute")]
[numthreads(16,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float3 origin,direction;
    bt_sample(probeKey,id.x,probeSampleInfo.x,origin,direction);
    probeSamples[id.x*2]=float4(origin,1);
    probeSamples[id.x*2+1]=float4(direction,0);
}

#include "../../../octaryn-client/Shaders/BlockTransportGI/Hit.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Cache.slang"
struct BtMaterialRay {float4 origin,direction;};
struct BtMaterialResult {uint4 identity;int4 key;float4 properties;uint4 admission;};
StructuredBuffer<BtMaterialRay> probeMaterialRays;
RWStructuredBuffer<BtMaterialResult> probeMaterialResults;
uniform uint4 probeMaterialInfo;
[shader("compute")]
[numthreads(64,1,1)]
void material_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=probeMaterialInfo.x)return;
    BtMaterialRay ray=probeMaterialRays[id.x];
    WorldRayHit hit=world_trace_ray(ray.origin.xyz,ray.direction.xyz,ray.origin.w,
        false,float3(1e30),false,false,true);
    int4 key;bool valid=bt_hit_key(hit,key);
    uint admitted=valid?bt_admit(key,hit.albedo):BT_INVALID;
    BtMaterialResult result;
    result.identity=uint4(hit.hit?1:0,hit.material,hit.flags,valid?1:0);
    result.key=key;result.properties=float4(hit.distance,hit.albedo.a,hit.backFace?1:0,0);
    result.admission=uint4(admitted,valid?bt_find(key):BT_INVALID,0,0);
    probeMaterialResults[id.x]=result;
}
