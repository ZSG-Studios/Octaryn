#include "../DDGI/DDGISample.slang"
RWStructuredBuffer<float4> result;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    const float x[4]={0,2,3.1,2.0001};
    float coverage;
    float3 value=ddgi_sample(float3(x[id.x],0,0),float3(0,1,0),coverage);
    result[id.x]=float4(value,coverage);
}
