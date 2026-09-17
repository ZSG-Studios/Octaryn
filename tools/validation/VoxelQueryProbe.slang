#include "../../octaryn-client/Shaders/VoxelTracing/VoxelQuery.slang"
#ifdef VOXEL_BUFFER_PROBE
#include "../../octaryn-client/Shaders/VoxelTracing/VoxelTraceBuffers.slang"
#endif
struct QueryCase {
    int4 originChunk;
    float4 originMin;
    float4 directionMax;
    int4 lowerChunk;
    int4 upperChunk;
    int4 occupiedChunk;
    int4 occupiedVoxel;
    int4 unknownChunk;
    uint4 options; // material, maximum steps, unknown enabled, reserved
};
struct QueryResult {int4 chunkStatus;int4 voxelMaterial;float4 distanceNormal;float4 exit;uint4 steps;uint4 epoch;};
StructuredBuffer<QueryCase> cases;
RWStructuredBuffer<QueryResult> results;
uint2 one_bit(uint bit) {return bit<32?uint2(1u<<bit,0):uint2(0,1u<<(bit-32));}
struct FixtureWorld : IVoxelTraceAccessor {
    QueryCase input;
    uint chunk(int3 coordinate,out uint token) {
        token=0;
        if(input.options.z!=0 && all(coordinate==input.unknownChunk.xyz))return VOXEL_CHUNK_UNKNOWN;
        return input.options.x!=0 && all(coordinate==input.occupiedChunk.xyz)?VOXEL_CHUNK_RESIDENT:VOXEL_CHUNK_EMPTY;
    }
    uint2 knownY(uint token) {return uint2(0,32);}
    uint2 epoch(uint token) {return uint2(9,1);}
    uint2 macroMask(uint token) {return one_bit(voxel_linear(uint3(input.occupiedVoxel.xyz)/16,2));}
    uint2 brickMask(uint token,uint macroIndex) {return one_bit(voxel_linear((uint3(input.occupiedVoxel.xyz)%16)/4,4));}
    uint2 voxelMask(uint token,uint brickIndex) {return one_bit(voxel_linear(uint3(input.occupiedVoxel.xyz)%4,4));}
    uint material(uint token,uint3 localVoxel) {return input.options.x;}
};
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    QueryCase input=cases[id.x];
#ifdef VOXEL_BUFFER_PROBE
    VoxelTraceBuffers world;
#else
    FixtureWorld world;world.input=input;
#endif
    VoxelTraceRay ray;
    ray.originChunk=input.originChunk.xyz;ray.origin=input.originMin.xyz;ray.direction=input.directionMax.xyz;
    ray.tMin=input.originMin.w;ray.tMax=input.directionMax.w;ray.maxSteps=input.options.y;
    VoxelTraceBounds bounds;bounds.lower=input.lowerChunk.xyz;bounds.upper=input.upperChunk.xyz;
    VoxelTraceResult hit=voxel_trace_candidate(world,ray,bounds);
    QueryResult result;
    result.chunkStatus=int4(hit.chunk,int(hit.status));result.voxelMaterial=int4(hit.voxel,int(hit.material));
    result.distanceNormal=float4(hit.distance,float3(hit.normal));result.exit=float4(hit.exitDistance,0,0,0);result.steps=hit.steps;
#ifdef VOXEL_BUFFER_PROBE
    result.epoch=uint4(hit.geometryEpoch,voxel_trace_generation());
#else
    result.epoch=uint4(hit.geometryEpoch,0,0);
#endif
    results[id.x]=result;
}
