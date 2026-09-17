#include "HitRadiance.slang"
SrcRay src_trace_sample(float3 position,float3 normal,uint lod,uint rng,float2 sample) {
    SrcRay ray=(SrcRay)0;ray.origin=float4(position,lod);
    float y=sample.y*2-1,a=sample.x*(2*SRC_PI),r=sqrt(max(0,1-y*y));
    float3 direction=float3(r*cos(a),y,r*sin(a));if(dot(direction,normal)<0)direction=-direction;
    float3 start=src_surface_start(position,normal);
    float contact=srcIntervals.y*srcIntervals.x*exp2(float(lod));
    SrcHit hit=src_trace(start,direction,contact,srcTemporal.z);InterlockedAdd(srcCounters[43],1);
    if(hit.status!=VOXEL_TRACE_CANDIDATE && hit.status!=VOXEL_TRACE_MISS)return ray;
    uint split=srcCounts.x-1;float3 value;
    if(hit.status==VOXEL_TRACE_CANDIDATE) {
        float distance=hit.surface.cell.distance;
        split=uint(clamp(ceil(log(max(distance/(srcIntervals.z*srcIntervals.x*exp2(float(lod))),1.0))/log(srcIntervals.w)),0.0,float(srcCounts.x-1)));
        value=src_hit_radiance(hit,rng);ray.hitPosition=float4(hit.position,1);ray.hitNormal=float4(hit.surface.normal,distance);
    } else {value=src_environment(direction);ray.hitNormal.w=-1;}
    ray.direction=float4(direction,split);ray.radiance=float4(value,1);return ray;
}
[shader("compute")][numthreads(64,1,1)]
void primary(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);if(id.x>=src_primary_count())return;
    float3 position,normal;SrcRay ray=(SrcRay)0;
    if(src_surface(src_pixel(id.x),position,normal)) {
        uint lod=src_lod(position),probe=src_find(src_cell(position,lod,0),lod,0);
        if(probe!=SRC_INVALID) {
            uint cursor;InterlockedAdd(srcDistribution[probe].z,1,cursor);uint index=srcDistribution[probe].y+cursor;
            uint rng=src_hash(srcExtent.z+968);float2 rotation=float2(src_random(rng),src_random(rng));
            uint2 fixed=uint2(3242174889u,2447445414u)*index;
            float2 sample=frac(float2(.5)+float2(fixed>>8)*(1.0/16777216.0)+rotation);
            ray=src_trace_sample(position,normal,lod,src_hash(id.x+srcExtent.z*9781+1),sample);
        }
    }
    srcRays[id.x]=ray;
}
[shader("compute")][numthreads(64,1,1)]
void secondary(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);if(id.x>=src_primary_count())return;
    SrcRay first=srcRays[id.x],ray=(SrcRay)0;
    if(first.hitPosition.w>0) {
        uint rng=src_hash(id.x+srcExtent.z*6271+137);float2 sample=float2(src_random(rng),src_random(rng));
        ray=src_trace_sample(first.hitPosition.xyz,first.hitNormal.xyz,src_lod(first.hitPosition.xyz)+2,rng,sample);
    }
    srcRays[src_primary_count()+id.x]=ray;
}
