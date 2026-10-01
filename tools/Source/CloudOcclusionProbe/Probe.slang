#include "../../../octaryn-client/Shaders/Sky/CloudOcclusionLib.slang"
StructuredBuffer<float4> positions;
StructuredBuffer<float4> cases;
RWStructuredBuffer<uint> results;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID)
{
    float4 c=cases[id.x];
    results[id.x]=cloud_occluded_before_entry(positions[id.x],c.x,c.y,c.zw)?1:0;
}
