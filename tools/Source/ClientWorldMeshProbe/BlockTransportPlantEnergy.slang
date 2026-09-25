#include "../../../octaryn-client/Shaders/BlockTransportGI/Sample.slang"
StructuredBuffer<uint4> energyKeys;
RWStructuredBuffer<float4> energyResults;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=3)return;
    float3 value;
    bool valid=sample_block_transport_gi(energyKeys[id.x],value);
    energyResults[id.x]=float4(value,valid?1:0);
}
