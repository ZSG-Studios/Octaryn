// DDGI probe overlay: billboard spheres at relocated probe centers. Irradiance
// mode shades the sphere from the probe's own octahedral map; state mode colors
// version/occupancy/update status so scroll and edit waves are visible live.
#include "DDGITypes.slang"
StructuredBuffer<DDGIControl> ddgiControls;
StructuredBuffer<DDGIProbe> ddgiProbes;
StructuredBuffer<float4> ddgiIrradiance;
StructuredBuffer<float> ddgiVariability;
uniform float4 camera[5];
uniform uint ddgiDebugMode; // 0 irradiance, 1 state, 2 convergence
struct Varying {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    nointerpolation uint probe:TEXCOORD1;
    nointerpolation float3 state:COLOR0;
};
float3 ddgi_debug_irradiance(uint probe,float3 direction) {
    uint n=ddgiFrame.z;float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiIrradiance[base+ddgi_texel(lo,n)].rgb,ddgiIrradiance[base+ddgi_texel(lo+int2(1,0),n)].rgb,f.x),
        lerp(ddgiIrradiance[base+ddgi_texel(lo+int2(0,1),n)].rgb,ddgiIrradiance[base+ddgi_texel(lo+1,n)].rgb,f.x),f.y);
}
[shader("vertex")]
Varying vertex_main(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
    Varying output=(Varying)0;
    output.probe=instance;
    DDGIControl control=ddgiControls[instance];DDGIProbe probe=ddgiProbes[instance];
    float anchor=float(ddgiOrigin.w)*.5;
    float3 center=(float3(control.cell)+anchor)*ddgiParameters.x+probe.offset.xyz;
    float radius=ddgiParameters.x*.28;
    // State coloring also reaches the irradiance mode through marker size.
    bool mismatch=probe.metadata.x!=control.version;
    bool solid=control.padding.x!=0,hole=control.padding.y!=0;
    if(solid)radius*=.6;
    float3 state=float3(.1,1,.2); // valid, converged
    if(probe.offset.w==1)state=float3(1,.1,.1); // relocated out (inactive)
    else if(probe.offset.w==2)state=float3(.1,.3,1); // sleeping
    else if(probe.metadata.w==0)state=float3(1,.7,0); // pending
    if(solid)state=float3(.6,.2,.2);
    else if(hole)state=float3(1,.85,.1); // speculative opening probe
    if(mismatch)state=float3(1,0,1); // wrapped slot, not yet re-seeded
    if(ddgiDebugMode==2) {
        // Convergence heat: green settled (low variability EMA), red churning,
        // blue asleep. Instant deltas and the stable streak both feed the tint.
        state=lerp(float3(.1,1,.2),float3(1,.15,.1),saturate(ddgiVariability[instance]*12));
        if((probe.metadata.z&65535u)==0&&probe.offset.w!=2)state=lerp(state,float3(1,.5,.1),.5);
        if(probe.offset.w==2)state=float3(.1,.3,1);
        else if(probe.offset.w==1)state=float3(.55);
        if(mismatch)state=float3(1,0,1);
    }
    // White flash marks probes traced within the last two frames: the moving
    // update wavefront is directly visible against the converged field.
    if(ddgiFrame.x-probe.metadata.y<=2)state=lerp(state,float3(1),.6);
    output.state=state;
    const float2 corners[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
    float2 uv=corners[vertex];
    float3 world=center+(camera[1].xyz*uv.x+camera[2].xyz*uv.y)*radius;
    float3 p=world-camera[0].xyz;
    float z=dot(p,camera[3].xyz);
    output.position=float4(dot(p,camera[1].xyz)*camera[4].x,dot(p,camera[2].xyz)*camera[4].y,
        z*camera[4].z-camera[4].w,z);
    output.position.xy+=float2(camera[1].w,camera[2].w)*z;
    output.uv=uv;
    return output;
}
[shader("fragment")]
float4 fragment_main(Varying input):SV_Target {
    float squared=dot(input.uv,input.uv);
    if(squared>1)discard;
    if(ddgiDebugMode!=0)return float4(input.state,1);
    float3 local=float3(input.uv.x,input.uv.y,sqrt(max(1-squared,0)));
    float3 direction=normalize(camera[1].xyz*local.x+camera[2].xyz*local.y+camera[3].xyz*local.z);
    float3 irradiance=ddgi_debug_irradiance(input.probe,direction)/3.14159265359;
    DDGIControl control=ddgiControls[input.probe];DDGIProbe probe=ddgiProbes[input.probe];
    // Non-converged or out-of-cage probes keep their state color so they stay
    // visible instead of vanishing into black in the irradiance views.
    if(control.padding.x!=0||control.padding.y!=0||probe.metadata.w==0||
       probe.metadata.x!=control.version)return float4(input.state,1);
    return float4(irradiance/(1+irradiance),1);
}
