#include "WorldRayQuery.slang"
Texture2D<float4> positions;
RWTexture2D<float4> scene;
uniform float4 eye;
uniform uint2 extent;
uniform uint debugMode;
float3 instance_color(uint instance) {
    uint h=(instance+1)*747796405u+2891336453u;
    h=((h>>((h>>28)+4))^h)*277803737u;h=(h>>22)^h;
    return .12+.88*float3(h&255,(h>>8)&255,(h>>16)&255)/255;
}
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent))return;
    float4 surface=positions[id.xy];
    if(surface.w<=0) {scene[id.xy]=float4(.008,.012,.02,1);return;}
    float distance=length(surface.xyz);
    if(distance<.0001)return;
    WorldRayHit hit=world_trace_ray(eye.xyz,surface.xyz/distance,min(distance+1,raySettings.y));
    float3 color=float3(1,0,.65); // Raster-visible surface absent from the ready TLAS.
    if(hit.hit) {
        color=instance_color(hit.instance);
        if(debugMode==11) {
            float t=saturate(log2(1+hit.distance)/log2(1+raySettings.y));
            color=saturate(float3(1.5-abs(4*t-3),1.5-abs(4*t-2),1.5-abs(4*t-1)));
        }
    }
    scene[id.xy]=float4(color,1);
}
