#include "../../../octaryn-client/Shaders/BlockTransportGI/WorldAdmission.slang"
ByteAddressBuffer worldExpansionFaces;
StructuredBuffer<uint4> worldExpansionRequests;
struct WorldExpansionResult {int4 key;float4 reflectance;uint4 state;};
RWStructuredBuffer<WorldExpansionResult> worldExpansionResults;
uniform uint worldExpansionCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=worldExpansionCount)return;
    uint4 request=worldExpansionRequests[id.x];
    uint4 face=worldExpansionFaces.Load4(request.x*16);
    WorldExpansionResult result;
    bool valid=bt_world_surface(face,request.y,result.key,result.reflectance);
    result.state=uint4(bt_world_surface_count(face),valid?1:0,0,0);
    worldExpansionResults[id.x]=result;
}
