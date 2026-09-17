#include "../../octaryn-client/Shaders/VoxelTracing/VoxelSegmentQuery.slang"
struct SegmentCase {
    float4 originTMin; // xyz anchor-relative local origin, w tMin
    float4 directionTMax; // xyz direction, w tMax
    int4 anchorSteps; // xyz anchorBlock, w maxSteps
};
struct SegmentResult {
    int4 voxelStatus; // xyz global voxel, w status
    int4 faceMaterial; // xyz entry face normal, w materialId
    float4 timeExit; // t, tExit
    uint4 steps;
    uint4 epochGeneration;
    int4 chunk; // xyz global chunk
};
StructuredBuffer<SegmentCase> cases;
RWStructuredBuffer<SegmentResult> results;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint id:SV_DispatchThreadID) {
    SegmentCase input=cases[id];
    VoxelTraceHit hit=voxel_trace_segment_budgeted(input.originTMin.xyz,input.directionTMax.xyz,
        input.originTMin.w,input.directionTMax.w,input.anchorSteps.xyz,uint(input.anchorSteps.w));
    SegmentResult result;
    result.voxelStatus=int4(hit.voxel,int(hit.status));
    result.faceMaterial=int4(hit.face,int(hit.materialId));
    result.timeExit=float4(hit.t,hit.tExit,0,0);
    result.steps=hit.steps;
    result.epochGeneration=uint4(hit.geometryEpoch,voxel_trace_generation());
    result.chunk=int4(hit.chunk,0);
    results[id]=result;
}
