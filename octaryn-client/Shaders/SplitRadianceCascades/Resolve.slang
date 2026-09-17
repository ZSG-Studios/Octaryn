#include "HitRadiance.slang"
struct SrcDonors {uint indices[8];float weights[8];};
SrcDonors src_donors(float3 position,uint lod) {
    SrcDonors donors;int3 base;float3 f;src_spatial(position,lod,0,base,f);
    for(uint i=0;i<8;++i) {
        donors.indices[i]=SRC_INVALID;donors.weights[i]=0;
        int3 o=int3(i&1,(i>>1)&1,(i>>2)&1);uint probe=src_find(base+o,lod,0);if(probe==SRC_INVALID)continue;
        if(!src_visible(position,src_probe_surface(probe,0))) {InterlockedAdd(srcCounters[45],1);continue;}
        float3 t=lerp(1-f,f,float3(o));donors.indices[i]=probe;
        donors.weights[i]=t.x*t.y*t.z*max(srcProbes[probe].quality,.001);
    }
    return donors;
}
float4 src_gather_oct(SrcDonors donors,float3 normal) {
    float4 sum=0;float total=0;
    for(uint i=0;i<8;++i) {
        uint probe=donors.indices[i];if(probe==SRC_INVALID)continue;
        float4 value=src_oct_sample(probe,normal);if(value.w<0)continue;
        sum+=float4(value.xyz,1)*donors.weights[i];total+=donors.weights[i];
    }
    return total>0?float4(sum.xyz/total,sum.w/total):float4(0,0,0,-1);
}
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=srcExtent.xy))return;float3 position,normal;
    if(!src_surface(id.xy,position,normal)) {srcLighting[id.xy]=0;return;}
    float3 start=src_surface_start(position,normal);uint lod=src_lod(position);
    SrcDonors low=src_donors(start,lod),high=low;float lowWeight=src_lod_weight(position,lod);
    if(lowWeight<1)high=src_donors(start,lod+1);
    float4 lowOct=src_gather_oct(low,normal),highOct=lowOct;
    if(lowWeight<1) {
        float4 far=src_gather_oct(high,normal);
        float total=(lowOct.w>=0?lowWeight:0)+(far.w>=0?1-lowWeight:0);
        lowOct=total>0?float4((max(lowOct.xyz,0)*lowWeight+max(far.xyz,0)*(1-lowWeight))/total,
            (lowOct.w*lowWeight+far.w*(1-lowWeight))/total):float4(0,0,0,-1);
    }
    float3 irradiance=0;float coverage=0,cosine=0;
    uint rng=src_hash(id.x+id.y*srcExtent.x+srcExtent.z*1361);
    // Contact detail below C0 spacing stays a short voxel DDA hemisphere query.
    for(uint d=0;d<srcCascades[0].directions;++d) {
        float3 direction=src_bin_direction(d,0);float w=max(dot(normal,direction),0);if(w<=0)continue;cosine+=w;
        float contact=srcIntervals.y*srcIntervals.x*exp2(float(lod));
        SrcHit hit=(SrcHit)0;hit.status=VOXEL_TRACE_MISS;
        if(contact>0)hit=src_trace(start,direction,0,contact);
        if(hit.status==VOXEL_TRACE_CANDIDATE) {irradiance+=src_hit_radiance(hit,rng)*w;coverage+=w;continue;}
        if(hit.status!=VOXEL_TRACE_MISS)continue;
        if(lowOct.w>=0) {irradiance+=lowOct.xyz*w;coverage+=w;}
    }
    // Zero-count directions do not enter normalization (paper section 5).
    srcLighting[id.xy]=float4(coverage>0?irradiance*(SRC_PI/coverage):float3(0),coverage/max(cosine,1e-6));
}
