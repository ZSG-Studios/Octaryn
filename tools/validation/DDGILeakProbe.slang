#include "DDGI/DDGISample.slang"
StructuredBuffer<float4> positions;
StructuredBuffer<float4> normals;
RWStructuredBuffer<float4> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float coverage;
    float3 value=ddgi_sample(positions[id.x].xyz,normals[id.x].xyz,coverage);
    results[id.x]=float4(value,coverage);
}
