#ifndef OCTARYN_VOXEL_SEGMENT_QUERY
#define OCTARYN_VOXEL_SEGMENT_QUERY
#include "VoxelTraceBuffers.slang"

// Shared segment query for Split Radiance Cascades rays, direct visibility and
// contact passes. World position = float(anchorBlock) + anchorRelative; all
// traversal identity stays integer-exact and rebasing never changes cache keys.
static const uint VOXEL_SEGMENT_INVALID=0;
static const uint VOXEL_SEGMENT_HIT=1;
static const uint VOXEL_SEGMENT_MISS=2;
static const uint VOXEL_SEGMENT_UNKNOWN=3;
static const uint VOXEL_SEGMENT_EXHAUSTED=4;
// Addressable window: matches the exact-float rebase limit of voxel_relative_chunk.
static const int VOXEL_SEGMENT_WINDOW_CHUNKS=8192;
static const uint VOXEL_SEGMENT_DEFAULT_STEPS=65536;

struct VoxelTraceHit {
    uint status;      // VOXEL_SEGMENT_*
    int3 voxel;       // Global block coordinate; valid on HIT.
    uint materialId;  // Occupancy candidate only: cutout/fluid/glass policy belongs
                      // to callers through atlas material flags, never to this query.
    int3 face;        // Entry face normal; zero when the segment starts inside the voxel.
    float t;          // Entry distance in blocks along normalize(dir); tMax on MISS.
    float tExit;      // Candidate voxel exit distance; resume here after a material rejection.
    int3 chunk;       // Global chunk coordinate of the candidate.
    uint2 geometryEpoch;
    uint4 steps;      // Chunk, macro, brick, voxel intervals visited.
};

int3 voxel_floor_div32(int3 value) {
    int3 rest=value%32;
    return value/32-int3(rest.x<0?1:0,rest.y<0?1:0,rest.z<0?1:0);
}
bool voxel_checked_add(int a,int b,out int result) {
    result=0;
    if(b>0 && a>2147483647-b)return false;
    if(b<0 && a<-2147483647-1-b)return false;
    result=a+b;return true;
}

// Rebase anchor-relative floats into the integer chunk frame of the origin block.
// originLocal-floor(originLocal) and the chunk-local offset are both exact, so no
// epsilon participates in boundary ownership. Local magnitudes stay below 2^22,
// where float32 integers and their floor conversions are exact.
bool voxel_segment_ray(float3 originLocal,float3 dir,float tMin,float tMax,
                       int3 anchorBlock,uint maxSteps,out VoxelTraceRay ray) {
    ray=(VoxelTraceRay)0;ray.maxSteps=maxSteps;
    if(maxSteps==0 || !all(isfinite(originLocal)) || !all(isfinite(dir)) ||
       !isfinite(tMin) || !isfinite(tMax) || tMin<0 || tMax<=tMin ||
       any(abs(originLocal)>=4194304.0))return false;
    float magnitude=length(dir);
    if(!(magnitude>0) || !isfinite(magnitude))return false;
    ray.direction=dir/magnitude;
    ray.tMin=tMin;ray.tMax=tMax;
    int3 localFloor=int3(floor(originLocal));
    int3 globalBlock;
    if(!voxel_checked_add(anchorBlock.x,localFloor.x,globalBlock.x) ||
       !voxel_checked_add(anchorBlock.y,localFloor.y,globalBlock.y) ||
       !voxel_checked_add(anchorBlock.z,localFloor.z,globalBlock.z))return false;
    ray.originChunk=voxel_floor_div32(globalBlock);
    int3 blockInChunk=globalBlock-ray.originChunk*32;
    ray.origin=(originLocal-float3(localFloor))+float3(blockInChunk);
    return true;
}

VoxelTraceHit voxel_trace_segment_budgeted(float3 originLocal,float3 dir,float tMin,float tMax,
                                           int3 anchorBlock,uint maxSteps) {
    VoxelTraceHit hit=(VoxelTraceHit)0;hit.status=VOXEL_SEGMENT_INVALID;
    VoxelTraceRay ray;
    if(!voxel_segment_ray(originLocal,dir,tMin,tMax,anchorBlock,maxSteps,ray))return hit;
    VoxelTraceBounds bounds;
    bounds.lower=ray.originChunk-VOXEL_SEGMENT_WINDOW_CHUNKS;
    bounds.upper=ray.originChunk+VOXEL_SEGMENT_WINDOW_CHUNKS;
    VoxelTraceBuffers world;
    VoxelTraceResult result=voxel_trace_candidate(world,ray,bounds);
    hit.chunk=result.chunk;hit.materialId=result.material;hit.face=result.normal;
    hit.t=result.distance;hit.tExit=result.exitDistance;
    hit.geometryEpoch=result.geometryEpoch;hit.steps=result.steps;
    if(result.status==VOXEL_TRACE_CANDIDATE) {
        hit.status=VOXEL_SEGMENT_HIT;
        hit.voxel=result.chunk*32+result.voxel;
    } else if(result.status==VOXEL_TRACE_MISS) {
        // Known-empty resident coverage through tMax; never an implicit sky answer.
        hit.status=VOXEL_SEGMENT_MISS;hit.materialId=0;hit.t=tMax;hit.tExit=tMax;
    } else if(result.status==VOXEL_TRACE_UNKNOWN) {
        hit.status=VOXEL_SEGMENT_UNKNOWN;hit.materialId=0;
    } else if(result.status==VOXEL_TRACE_EXHAUSTED) {
        hit.status=VOXEL_SEGMENT_EXHAUSTED;hit.materialId=0;
    }
    return hit;
}

// tMin/tMax are block distances along normalize(dir). Returns true only for a
// definite occupied-material candidate; callers classify opaque versus
// transmissive from atlas flags for hit.materialId and may resume at hit.tExit.
bool voxel_trace_segment(float3 originLocalRelativeToIntegerAnchor,float3 dir,float tMin,float tMax,
                         int3 anchorBlock,out VoxelTraceHit hit) {
    hit=voxel_trace_segment_budgeted(originLocalRelativeToIntegerAnchor,dir,tMin,tMax,
                                     anchorBlock,VOXEL_SEGMENT_DEFAULT_STEPS);
    return hit.status==VOXEL_SEGMENT_HIT;
}
#endif
