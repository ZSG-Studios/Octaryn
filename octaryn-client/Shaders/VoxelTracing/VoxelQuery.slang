#ifndef OCTARYN_VOXEL_QUERY
#define OCTARYN_VOXEL_QUERY
#include "VoxelDda.slang"

static const uint VOXEL_TRACE_MISS=0;
static const uint VOXEL_TRACE_CANDIDATE=1;
static const uint VOXEL_TRACE_UNKNOWN=2;
static const uint VOXEL_TRACE_EXHAUSTED=3;
static const uint VOXEL_TRACE_INVALID=4;
static const uint VOXEL_CHUNK_EMPTY=0;
static const uint VOXEL_CHUNK_RESIDENT=1;
static const uint VOXEL_CHUNK_UNKNOWN=2;

// This accessor separates traversal from the resident world's buffer layout.
// Masks are 64 logical bits in uint2; x-fastest, then y, then z at every level.
interface IVoxelTraceAccessor {
    uint chunk(int3 coordinate,out uint token);
    uint2 knownY(uint token);
    uint2 epoch(uint token);
    uint2 macroMask(uint token);
    uint2 brickMask(uint token,uint macroIndex);
    uint2 voxelMask(uint token,uint brickIndex);
    uint material(uint token,uint3 localVoxel);
};

struct VoxelTraceRay {
    int3 originChunk;
    float3 origin; // Relative to originChunk * 32, not a global float position.
    float3 direction; // Unit length: interval and returned distance are in blocks.
    float tMin;
    float tMax;
    uint maxSteps;
};

struct VoxelTraceBounds {
    int3 lower; // Addressable window: outside is UNKNOWN, never an implicit sky miss.
    int3 upper;
};

struct VoxelTraceResult {
    uint status;
    int3 chunk;
    int3 voxel; // 0..31 in chunk, valid for CANDIDATE.
    uint material;
    uint2 geometryEpoch;
    float distance;
    float exitDistance;
    int3 normal;
    uint4 steps; // Chunk, macro, brick, voxel intervals visited.
};

bool voxel_mask_bit(uint2 mask,uint bit) {
    return ((bit<32?mask.x:mask.y)&(1u<<(bit&31)))!=0;
}
uint voxel_linear(uint3 coordinate,uint side) {
    return coordinate.x+side*(coordinate.y+side*coordinate.z);
}
VoxelRayInterval voxel_child_interval(VoxelDda parent) {
    VoxelRayInterval interval;
    interval.begin=parent.time; interval.end=voxel_dda_exit(parent); interval.normal=parent.normal;
    return interval;
}
bool voxel_relative_chunk(int3 coordinate,int3 anchor,out int3 relative) {
    relative=0;
    for(uint axis=0;axis<3;++axis) {
        bool positive=coordinate[axis]>=anchor[axis];
        uint distance=positive?asuint(coordinate[axis])-asuint(anchor[axis]):
            asuint(anchor[axis])-asuint(coordinate[axis]);
        if(distance>262144u)return false;
        relative[axis]=positive?int(distance):-int(distance);
    }
    return true;
}

// Returns occupied voxel candidates, not an assertion that a cutout, fluid or
// shaped material fills its whole cell. The caller refines geometry within the
// returned [distance,exitDistance) and resumes at exitDistance after rejection.
VoxelTraceResult voxel_trace_candidate<T:IVoxelTraceAccessor>(T world,VoxelTraceRay ray,VoxelTraceBounds bounds) {
    VoxelTraceResult result=(VoxelTraceResult)0;result.status=VOXEL_TRACE_INVALID;
    if(ray.maxSteps==0 || !all(isfinite(ray.origin)) || !all(isfinite(ray.direction)) ||
       !isfinite(ray.tMin) || !isfinite(ray.tMax) || ray.tMin<0 || ray.tMax<=ray.tMin ||
       abs(dot(ray.direction,ray.direction)-1)>1e-4 || any(bounds.upper<=bounds.lower))return result;
    // Unsigned differences avoid signed overflow across zero; float conversion
    // happens only after integer rebasing and finite-window validation.
    int3 lowerChunk,upperChunk;
    if(!voxel_relative_chunk(bounds.lower,ray.originChunk,lowerChunk) ||
       !voxel_relative_chunk(bounds.upper,ray.originChunk,upperChunk))return result;
    int3 lower=lowerChunk*32,upper=upperChunk*32;
    VoxelRayInterval interval;
    result.status=VOXEL_TRACE_UNKNOWN;result.distance=ray.tMin;
    if(!voxel_clip_ray(ray.origin,ray.direction,float3(lower),float3(upper),ray.tMin,ray.tMax,interval) ||
       interval.begin>ray.tMin)return result;
    result.status=VOXEL_TRACE_MISS;
    VoxelDda chunks=voxel_dda_begin(ray.origin,ray.direction,interval,32,lower,upper);
    uint remaining=ray.maxSteps;
    do {
        if(remaining==0) {result.status=VOXEL_TRACE_EXHAUSTED;return result;}--remaining;++result.steps.x;
        if(chunks.time>=voxel_dda_exit(chunks))continue;
        result.chunk=ray.originChunk+chunks.cell/32;
        result.distance=chunks.time;result.exitDistance=voxel_dda_exit(chunks);
        uint token;uint residency=world.chunk(result.chunk,token);
        if(residency==VOXEL_CHUNK_UNKNOWN) {result.status=VOXEL_TRACE_UNKNOWN;return result;}
        if(residency==VOXEL_CHUNK_EMPTY)continue;
        uint2 known=world.knownY(token);
        int3 knownLower=chunks.cell+int3(0,int(known.x),0);
        int3 knownUpper=chunks.cell+int3(32,int(known.y),32);
        VoxelRayInterval knownInterval;
        if(!voxel_clip_ray(ray.origin,ray.direction,float3(knownLower),float3(knownUpper),
                          chunks.time,voxel_dda_exit(chunks),knownInterval) || knownInterval.begin>chunks.time) {
            result.status=VOXEL_TRACE_UNKNOWN;return result;
        }
        knownInterval.normal=chunks.normal;
        uint2 macros=world.macroMask(token);
        VoxelDda macro=voxel_dda_begin(ray.origin,ray.direction,knownInterval,16,chunks.cell,chunks.cell+32);
        do {
            if(remaining==0) {result.status=VOXEL_TRACE_EXHAUSTED;return result;}--remaining;++result.steps.y;
            if(macro.time>=voxel_dda_exit(macro))continue;
            uint macroIndex=voxel_linear(uint3((macro.cell-chunks.cell)/16),2);
            if(!voxel_mask_bit(macros,macroIndex))continue;
            uint2 bricks=world.brickMask(token,macroIndex);
            VoxelDda brick=voxel_dda_begin(ray.origin,ray.direction,voxel_child_interval(macro),4,macro.cell,macro.cell+16);
            do {
                if(remaining==0) {result.status=VOXEL_TRACE_EXHAUSTED;return result;}--remaining;++result.steps.z;
                if(brick.time>=voxel_dda_exit(brick))continue;
                uint brickInMacro=voxel_linear(uint3((brick.cell-macro.cell)/4),4);
                if(!voxel_mask_bit(bricks,brickInMacro))continue;
                uint brickIndex=voxel_linear(uint3((brick.cell-chunks.cell)/4),8);
                uint2 voxels=world.voxelMask(token,brickIndex);
                VoxelDda voxel=voxel_dda_begin(ray.origin,ray.direction,voxel_child_interval(brick),1,brick.cell,brick.cell+4);
                do {
                    if(remaining==0) {result.status=VOXEL_TRACE_EXHAUSTED;return result;}--remaining;++result.steps.w;
                    if(voxel.time>=voxel_dda_exit(voxel))continue;
                    uint bit=voxel_linear(uint3(voxel.cell-brick.cell),4);
                    if(!voxel_mask_bit(voxels,bit))continue;
                    result.status=VOXEL_TRACE_CANDIDATE;result.voxel=voxel.cell-chunks.cell;
                    result.material=world.material(token,uint3(result.voxel));
                    result.geometryEpoch=world.epoch(token);
                    result.distance=voxel.time;result.exitDistance=voxel_dda_exit(voxel);result.normal=voxel.normal;
                    return result;
                } while(voxel_dda_step(voxel));
            } while(voxel_dda_step(brick));
        } while(voxel_dda_step(macro));
        if(knownInterval.end<voxel_dda_exit(chunks)) {
            result.status=VOXEL_TRACE_UNKNOWN;result.distance=knownInterval.end;return result;
        }
    } while(voxel_dda_step(chunks));
    if(interval.end<ray.tMax) {result.status=VOXEL_TRACE_UNKNOWN;result.distance=interval.end;}
    return result;
}
#endif
