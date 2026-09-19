#include "../../octaryn-client/Shaders/DDGI/DDGISample.slang"
#include "../../octaryn-client/Shaders/DDGI/DDGIIrradiance.slang"
struct TransitionCase {float3 position;float source;float bounce;uint reactive;uint reset;};
StructuredBuffer<TransitionCase> cases;
RWStructuredBuffer<float4> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    TransitionCase c=cases[id.x];float coverage,feedbackCoverage;
    float3 display=ddgi_sample(c.position,float3(0,1,0),coverage);
    float3 feedback=ddgi_sample(c.position,float3(0,1,0),feedbackCoverage,true);
    uint index=ddgi_index(int3(3,3,3));
    DDGIControl control=ddgiControls[index];DDGIProbe probe=ddgiProbes[index];
    bool changed=control.refreshFrame>probe.metadata.y;
    bool geometryChanged=changed&&(control.padding.z&4u)==0;
    bool removed=changed&&(control.padding.z&2u)!=0;
    float h=c.reset!=0||geometryChanged||removed?0:
        ddgi_history_retention(.94,probe.metadata.w,c.reactive,c.reset!=0);
    float3 incoming=3.14159265359*c.source+c.bounce*feedback*feedbackCoverage;
    results[id.x*3]=float4(display,coverage);
    results[id.x*3+1]=float4(feedback,feedbackCoverage);
    results[id.x*3+2]=float4(ddgi_blend_irradiance(incoming,display,h).x,h,
        ddgi_geometry_locked(control,probe,c.reset!=0)?1:0,0);
}
