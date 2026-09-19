#include "../../octaryn-client/Shaders/Shadows/Reprojection.slang"
struct Case {
    float3 old;
    float3 world;
    float3 eye;
    float3 ray;
    uint voxel;
    float tolerance;
};
StructuredBuffer<Case> cases;
RWStructuredBuffer<uint> results;
[shader("compute")][numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    Case value=cases[id.x];
    results[id.x]=shadow_history_position(value.old,value.world,value.voxel,
        value.eye,value.ray,value.tolerance)?1:0;
}
