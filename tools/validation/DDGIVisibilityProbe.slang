#include "../../octaryn-client/Shaders/DDGI/DDGISample.slang"
StructuredBuffer<float4> positions;
RWStructuredBuffer<float4> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float coverage;
    float3 value=ddgi_sample(positions[id.x].xyz,float3(0,1,0),coverage);
    results[id.x]=float4(value,coverage);
}
