#include "../../../octaryn-client/Shaders/BlockTransportGI/Reconstruct.slang"
#include "../../../octaryn-client/Shaders/Voxel/WorldSurfaceKey.slang"
struct Query {uint4 key;float4 uv;uint4 face;float4 local;};
struct Result {float4 reconstructed;float4 exact;uint4 key;float4 uv;};
StructuredBuffer<Query> probeQueries;
RWStructuredBuffer<Result> probeResults;
uniform uint4 probeInfo;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=probeInfo.x)return;
    Query query=probeQueries[id.x];Result result;
    float3 value;bool valid=reconstruct_block_transport_gi(query.key,query.uv.xy,value);
    result.reconstructed=float4(value,valid?1:0);
    valid=sample_block_transport_gi(query.key,value);result.exact=float4(value,valid?1:0);
    float2 uv;result.key=world_surface_address(query.face,query.local.xyz,uv);
    result.uv=float4(uv,0,0);probeResults[id.x]=result;
}
