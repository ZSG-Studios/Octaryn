#include "../../octaryn-client/Shaders/VoxelTracing/VoxelDda.slang"
struct DdaCase {
    float4 originMin;
    float4 directionMax;
    int4 lowerSize;
    int4 upper;
    int4 occupied;
};
struct DdaResult { float4 distanceNormal; int4 cellStatus; uint4 counts; };
StructuredBuffer<DdaCase> cases;
RWStructuredBuffer<DdaResult> results;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    DdaCase input=cases[id.x];DdaResult result=(DdaResult)0;
    result.cellStatus.w=2;
    VoxelRayInterval interval;
    if(voxel_clip_ray(input.originMin.xyz,input.directionMax.xyz,float3(input.lowerSize.xyz),
                     float3(input.upper.xyz),input.originMin.w,input.directionMax.w,interval)) {
        VoxelDda dda=voxel_dda_begin(input.originMin.xyz,input.directionMax.xyz,interval,
                                   input.lowerSize.w,input.lowerSize.xyz,input.upper.xyz);
        do {
            ++result.counts.x;
            if(dda.time<voxel_dda_exit(dda) && all(dda.cell==input.occupied.xyz)) {
                result.distanceNormal=float4(dda.time,float3(dda.normal));
                result.cellStatus=int4(dda.cell,1);break;
            }
            if(result.counts.x>=100000) {result.cellStatus.w=3;break;}
        } while(voxel_dda_step(dda));
    }
    results[id.x]=result;
}
