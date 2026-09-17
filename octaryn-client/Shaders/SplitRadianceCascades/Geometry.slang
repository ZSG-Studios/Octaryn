#ifndef OCTARYN_SRC_GEOMETRY
#define OCTARYN_SRC_GEOMETRY
#include "Resources.slang"
#include "../VoxelTracing/VoxelSurfaceQuery.slang"
struct SrcHit {VoxelSurfaceHit surface;float3 position;uint status;};
float3 src_surface_start(float3 position,float3 normal) {
    // One representable step, rather than a distance-scaled world epsilon.
    for(uint i=0;i<3;++i)if(normal[i]!=0 && position[i]!=0) {
        uint bits=asuint(position[i]);bits+=(position[i]>0)==(normal[i]>0)?1u:0xffffffffu;position[i]=asfloat(bits);
    }
    return position;
}
SrcHit src_trace(float3 position,float3 direction,float minimum,float maximum) {
    SrcHit hit=(SrcHit)0;hit.status=VOXEL_TRACE_INVALID;
    VoxelTraceRay ray;ray.originChunk=srcOrigin.xyz>>5;ray.origin=float3(srcOrigin.xyz&31)+position;
    ray.direction=direction;ray.tMin=minimum;ray.tMax=maximum;ray.maxSteps=8192;
    int radius=int(ceil((maximum+length(position))/32))+2;
    VoxelTraceBounds bounds;bounds.lower=ray.originChunk-radius;bounds.upper=ray.originChunk+radius+1;
    for(uint surfaces=0;surfaces<32;++surfaces) {
        VoxelSurfaceHit surface=voxel_trace_surface(ray,bounds);hit.surface=surface;hit.status=surface.cell.status;
        hit.position=position+direction*surface.cell.distance;
        uint4 steps=surface.cell.steps;
        InterlockedAdd(srcCounters[44],steps.x+steps.y+steps.z+steps.w);
        if(hit.status!=VOXEL_TRACE_CANDIDATE) {
            if(hit.status!=VOXEL_TRACE_MISS)InterlockedAdd(srcCounters[42],1);
            return hit;
        }
        // Match the shared material visibility policy: non-lava fluid and glass
        // transmit diffuse visibility. Alpha-tested geometry was refined by HDDA.
        bool transmit=((surface.flags&4)!=0&&(surface.flags&512)==0)||
            ((surface.flags&16)==0&&(surface.flags&2)==0&&(surface.flags&512)==0);
        if(!transmit)return hit;
        ray.tMin=surface.cell.exitDistance;
        if(ray.tMin>=maximum) {hit.status=VOXEL_TRACE_MISS;return hit;}
        uint used=steps.x+steps.y+steps.z+steps.w;
        ray.maxSteps-=min(ray.maxSteps,used);if(ray.maxSteps==0)break;
    }
    hit.status=VOXEL_TRACE_EXHAUSTED;InterlockedAdd(srcCounters[42],1);return hit;
}
bool src_visible(float3 a,float3 b) {
    float3 delta=b-a;float distance=length(delta);if(distance<1e-6)return true;
    // The end point is excluded by the HDDA half-open segment contract.
    SrcHit hit=src_trace(a,delta/distance,0,distance);return hit.status==VOXEL_TRACE_MISS;
}
float3 src_probe_surface(uint probe,uint cascade) {
    SrcProbe p=srcProbes[probe];return src_surface_start(src_anchor(p,cascade),p.normal.xyz);
}
#endif
