#ifndef OCTARYN_SRC_GEOMETRY
#define OCTARYN_SRC_GEOMETRY
#include "Resources.slang"
#include "../VoxelTracing/VoxelSurfaceQuery.slang"
struct SrcHit {VoxelSurfaceHit surface;float3 position;uint status;};
// Per-thread diagnostic accumulators. Entries flush once, so high-frequency
// Resolve/Trace/Links traces never serialize on shared counter addresses.
static uint4 g_srcStats=uint4(0,0,0,0); // steps, unresolved traces, occluded links
static uint4 g_srcStats2=uint4(0,0,0,0); // donor finds, donor misses, oct invalid, contact candidates
static uint4 g_srcStats3=uint4(0,0,0,0); // resolve pixels, zero-donor, gated, contact traces
static uint4 g_srcStats4=uint4(0,0,0,0); // unknown/exhausted contacts, reserved
void src_stats_flush() {
    if(g_srcStats.x!=0)InterlockedAdd(srcCounters[44],g_srcStats.x);
    if(g_srcStats.y!=0)InterlockedAdd(srcCounters[42],g_srcStats.y);
    if(g_srcStats.z!=0)InterlockedAdd(srcCounters[45],g_srcStats.z);
    g_srcStats=uint4(0,0,0,0);
}
// Resolve-only flush: keeps per-pass attribution separable in the profile dump.
void src_stats_flush_resolve() {
    src_stats_flush();
    if(g_srcStats2.x!=0)InterlockedAdd(srcCounters[78],g_srcStats2.x);
    if(g_srcStats2.y!=0)InterlockedAdd(srcCounters[79],g_srcStats2.y);
    if(g_srcStats2.z!=0)InterlockedAdd(srcCounters[80],g_srcStats2.z);
    if(g_srcStats2.w!=0)InterlockedAdd(srcCounters[81],g_srcStats2.w);
    if(g_srcStats3.x!=0)InterlockedAdd(srcCounters[82],g_srcStats3.x);
    if(g_srcStats3.y!=0)InterlockedAdd(srcCounters[83],g_srcStats3.y);
    if(g_srcStats3.z!=0)InterlockedAdd(srcCounters[84],g_srcStats3.z);
    if(g_srcStats3.w!=0)InterlockedAdd(srcCounters[85],g_srcStats3.w);
    if(g_srcStats4.x!=0)InterlockedAdd(srcCounters[86],g_srcStats4.x);
    g_srcStats2=uint4(0,0,0,0);g_srcStats3=uint4(0,0,0,0);g_srcStats4=uint4(0,0,0,0);
}
void src_stats_blocked() {++g_srcStats.z;}
float3 src_surface_start(float3 position,float3 normal) {
    // One representable step, rather than a distance-scaled world epsilon.
    for(uint i=0;i<3;++i)if(normal[i]!=0 && position[i]!=0) {
        uint bits=asuint(position[i]);bits+=(position[i]>0)==(normal[i]>0)?1u:0xffffffffu;position[i]=asfloat(bits);
    }
    return position;
}
SrcHit src_trace_budget(float3 position,float3 direction,float minimum,float maximum,uint budget) {
    SrcHit hit=(SrcHit)0;hit.status=VOXEL_TRACE_INVALID;
    VoxelTraceRay ray;ray.originChunk=srcOrigin.xyz>>5;ray.origin=float3(srcOrigin.xyz&31)+position;
    ray.direction=direction;ray.tMin=minimum;ray.tMax=maximum;ray.maxSteps=budget;
    int radius=int(ceil((maximum+length(position))/32))+2;
    VoxelTraceBounds bounds;bounds.lower=ray.originChunk-radius;bounds.upper=ray.originChunk+radius+1;
    for(uint surfaces=0;surfaces<32;++surfaces) {
        VoxelSurfaceHit surface=voxel_trace_surface(ray,bounds);hit.surface=surface;hit.status=surface.cell.status;
        hit.position=position+direction*surface.cell.distance;
        uint4 steps=surface.cell.steps;
        g_srcStats.x+=steps.x+steps.y+steps.z+steps.w;
        if(hit.status!=VOXEL_TRACE_CANDIDATE) {
            if(hit.status!=VOXEL_TRACE_MISS)++g_srcStats.y;
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
    hit.status=VOXEL_TRACE_EXHAUSTED;++g_srcStats.y;return hit;
}
SrcHit src_trace(float3 position,float3 direction,float minimum,float maximum) {
    return src_trace_budget(position,direction,minimum,maximum,8192);
}
uint src_entry_face(int3 normal) {
    if(normal.x<0)return 0;if(normal.x>0)return 1;
    if(normal.y<0)return 2;if(normal.y>0)return 3;
    if(normal.z<0)return 4;if(normal.z>0)return 5;return 6;
}
bool src_visible(float3 a,float3 b) {
    float3 delta=b-a;float distance=length(delta);if(distance<1e-6)return true;
    // The end point is excluded by the HDDA half-open segment contract. This is
    // a candidate-level query, deliberately lighter than src_trace: the shared
    // radiance policy transmits glass, water and cutout foliage, so only opaque
    // cubes, lava and sprite geometry can block donor visibility.
    VoxelTraceBuffers world;
    VoxelTraceRay ray;ray.originChunk=srcOrigin.xyz>>5;ray.origin=float3(srcOrigin.xyz&31)+a;
    ray.direction=delta/distance;ray.tMin=0;ray.tMax=distance;ray.maxSteps=8192;
    int radius=int(ceil((distance+length(a))/32))+2;
    VoxelTraceBounds bounds;bounds.lower=ray.originChunk-radius;bounds.upper=ray.originChunk+radius+1;
    for(uint passes=0;passes<64;++passes) {
        VoxelTraceResult cell=voxel_trace_candidate(world,ray,bounds);
        g_srcStats.x+=cell.steps.x+cell.steps.y+cell.steps.z+cell.steps.w;
        if(cell.status!=VOXEL_TRACE_CANDIDATE) {
            if(cell.status!=VOXEL_TRACE_MISS)++g_srcStats.y;
            return cell.status==VOXEL_TRACE_MISS;
        }
        if(cell.material>=voxelTraceMaterialCount) {++g_srcStats.y;return false;}
        uint flags=blockMaterials[cell.material].flags;
        bool blocking=(flags&512)!=0||(flags&2)!=0||((flags&(1|2|4|16|128))==(1|16));
        if(blocking) {
            // Buried entry faces transmit exactly like the full surface refinement
            // does; unknown neighbors stay conservatively occluded.
            uint face=src_entry_face(cell.normal);
            if(face>=6)return false;
            bool known;
            if(voxel_surface_visible(cell,face,known))return false;
            if(!known) {++g_srcStats.y;return false;}
        }
        ray.tMin=cell.exitDistance;
        if(ray.tMin>=distance)return true;
        uint used=cell.steps.x+cell.steps.y+cell.steps.z+cell.steps.w;
        ray.maxSteps-=min(ray.maxSteps,used);if(ray.maxSteps==0)break;
    }
    ++g_srcStats.y;return false; // Traversal budget spent: conservatively occluded.
}
float3 src_probe_surface(uint probe,uint cascade) {
    SrcProbe p=srcProbes[probe];return src_surface_start(src_anchor(p,cascade),p.normal.xyz);
}
#endif
