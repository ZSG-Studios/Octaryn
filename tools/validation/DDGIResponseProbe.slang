#include "../../octaryn-client/Shaders/DDGI/DDGIIrradiance.slang"
struct ResponseCase {
    float3 previous;
    float3 direction;
    uint first;
    uint count;
    uint samples;
    uint reactive;
    uint reset;
};
StructuredBuffer<ResponseCase> cases;
StructuredBuffer<DDGIRay> rays;
RWStructuredBuffer<float4> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    ResponseCase c=cases[id.x];
    float3 incoming=ddgi_integrate_irradiance(rays,0,c.first,c.count,c.direction);
    float history=ddgi_history_retention(.94,c.samples,c.reactive,c.reset!=0||c.samples==0);
    results[id.x]=float4(ddgi_blend_irradiance(incoming,c.previous,history),history);
}
