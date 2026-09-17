#include "Resources.slang"
// Fixed-point Q16 accumulation: one integer atomic per channel, no CAS retries.
// The device reports no float atomics, and compare-exchange storms livelock under
// debug layers. Contributions are clamped to [0,16) before scaling.
void src_add_component(uint address,uint component,float value) {
    const float scaled=clamp(value,0.0,16.0)*65536.0;
    InterlockedAdd(srcFluence[address][component],uint(scaled));
}
void src_add_weight(uint address) {
    InterlockedAdd(srcWeights[address],1u);
}
[shader("compute")][numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=2*src_primary_count())return;SrcRay ray=srcRays[id.x];if(ray.radiance.w==0)return;
    uint lod=uint(ray.origin.w),split=uint(ray.direction.w);
    uint lodCount=id.x<src_primary_count()&&src_lod_weight(ray.origin.xyz,lod)<1?2:1;
    for(uint l=0;l<lodCount;++l) {
      uint hitCascade=ray.hitNormal.w<0?srcCounts.x-1:uint(clamp(ceil(log(max(ray.hitNormal.w/(srcIntervals.z*srcIntervals.x*exp2(float(lod+l))),1.0))/log(srcIntervals.w)),0.0,float(srcCounts.x-1)));
      for(uint cascade=0;cascade<=hitCascade;++cascade) {
        uint probe=src_find(src_cell(ray.origin.xyz,lod+l,cascade),lod+l,cascade);if(probe==SRC_INVALID)continue;
        uint address=src_address(probe,cascade,src_direction(ray.direction.xyz,cascade));
        float4 interval=cascade==hitCascade?float4(ray.radiance.xyz,0):float4(0,0,0,1);
        for(uint channel=0;channel<4;++channel)src_add_component(address,channel,interval[channel]);
        src_add_weight(address);
        // Quality counts use a uint to avoid introducing float atomic capabilities.
        InterlockedAdd(srcProbes[probe].reserved,1);
      }
    }
}
