#include "../../../octaryn-client/Shaders/BlockTransportGI/Reflection.slang"
struct BtReflectionQuery {uint4 key;float4 material,parameters,normal;};
struct BtReflectionResult {float4 incident,primary,sun,fullSun;};
StructuredBuffer<BtReflectionQuery> probeQueries;
RWStructuredBuffer<BtReflectionResult> probeResults;
uniform uint probeCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=probeCount)return;
    BtReflectionQuery query=probeQueries[id.x];BtReflectionResult result;
    float3 incident;
    bool complete=sample_block_transport_reflected_incident(query.key,query.parameters.z!=0,incident);
    result.incident=float4(incident,complete?1:0);
    float3 primary;bool valid=sample_block_transport_gi(query.key,primary);
    result.primary=float4(primary,valid?1:0);
    float3 normal=query.normal.xyz;
    result.sun=float4(bt_reflected_sun_brdf(complete,query.material.rgb,query.material.w,
        query.parameters.x,normal,normal,normal)*query.parameters.y,0);
    result.fullSun=float4(pbr_direct(query.material.rgb,query.material.w,query.parameters.x,
        normal,normal,normal)*query.parameters.y,0);
    probeResults[id.x]=result;
}
