#include "LocalLighting.slang"
RWStructuredBuffer<Reservoir> result;
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent.xy))return;
    uint index=id.y*extent.x+id.x,rng=restir_hash(index^restir_hash(extent.z+1));
    LocalSurface s=local_surface(id.xy);Reservoir r=(Reservoir)0;
    if(s.valid>0 && extent.w>0)for(uint c=0;c<restirSettings.x;++c) {
        uint light=min(uint(restir_random(rng)*extent.w),extent.w-1);
        float2 sample=float2(restir_random(rng),restir_random(rng));
        float target=local_target(s,light,sample);
        reservoir_add(r,light,sample,target,target*extent.w,1,0,0,rng);
    }
    result[index]=r;
}
