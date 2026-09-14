#include "LocalLighting.slang"
StructuredBuffer<Reservoir> initial;
RWStructuredBuffer<Reservoir> result;
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent.xy))return;
    uint index=id.y*extent.x+id.x,rng=restir_hash(index^restir_hash(extent.z+1237));
    Reservoir r=initial[index];LocalSurface s=local_surface(id.xy);uint acceptance=r.reuse&4;
    if(s.valid>0 && extent.w>0)for(uint i=0;i<restirSettings.y;++i) {
        float angle=restir_random(rng)*6.283185307;
        float radius=sqrt(restir_random(rng))*reuseSettings.z;
        int2 p=int2(id.xy)+int2(round(float2(cos(angle),sin(angle))*radius));
        if(any(p<0)||any(p>=int2(extent.xy))||all(p==int2(id.xy)))continue;
        if(!compatible_surface(s,local_surface(p),false))continue;
        Reservoir source=initial[p.y*extent.x+p.x];
        if(source.M>0 && source.light<extent.w) {
            reservoir_merge(r,source,local_target(s,source.light,source.sample),reuseSettings.w,2,rng);
            acceptance|=8;
        }
    }
    r.reuse=(r.reuse&3)|acceptance;result[index]=r;
}
