#include "LocalLighting.slang"
StructuredBuffer<Reservoir> initial,history;
StructuredBuffer<LocalHistory> surfaceHistory;
RWStructuredBuffer<Reservoir> result;
uniform float4 previousPosition,previousRight,previousUp,previousForward,previousProjection;
uniform float4 previousJitter;
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent.xy))return;
    uint index=id.y*extent.x+id.x,rng=restir_hash(index^restir_hash(extent.z+719));
    Reservoir r=initial[index];LocalSurface s=local_surface(id.xy);
    float3 relative=s.position-previousPosition.xyz;
    float z=dot(relative,previousForward.xyz);
    float2 ndc=float2(dot(relative,previousRight.xyz)*previousProjection.x,dot(relative,previousUp.xyz)*previousProjection.y)/max(z,1e-5);
    ndc+=previousJitter.xy;
    int2 p=int2(floor(float2(ndc.x*.5+.5,.5-ndc.y*.5)*float2(extent.xy)));
    if(restirSettings.z!=0 && s.valid>0 && z>0 && all(p>=0) && all(p<int2(extent.xy))) {
        uint oldIndex=p.y*extent.x+p.x;
        Reservoir old=history[oldIndex];
        if(old.M>0 && old.light<extent.w && compatible_history(s,surfaceHistory[oldIndex])) {
            reservoir_merge(r,old,local_target(s,old.light,old.sample),reuseSettings.w,1,rng);
            r.reuse|=4;
        }
    }
    result[index]=r;
}
