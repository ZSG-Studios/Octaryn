#include "DDGI/DDGISample.slang"
StructuredBuffer<float4> positions;
StructuredBuffer<float4> normals;
RWStructuredBuffer<float4> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float3 position=positions[id.x].xyz,normal=normals[id.x].xyz;
    float coverage;
    float3 value=ddgi_sample(position,normal,coverage);
    results[id.x]=float4(value,coverage);
    value=ddgi_sample_volume(position,normal,coverage,ddgiFineGrid,ddgiFineOrigin,
        ddgiFineFadeOrigin,ddgiFineParameters,ddgiFineFrame,ddgiFineControls,ddgiFineProbes,
        ddgiFineIrradiance,ddgiFineDistance);
    results[12+id.x]=float4(value,coverage);
    value=ddgi_sample_volume(position,normal,coverage,ddgiGrid,ddgiOrigin,
        ddgiFadeOrigin,ddgiParameters,ddgiFrame,ddgiControls,ddgiProbes,ddgiIrradiance,ddgiDistance);
    results[24+id.x]=float4(value,coverage);
}
