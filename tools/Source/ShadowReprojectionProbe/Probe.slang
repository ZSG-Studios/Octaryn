#include "../../../octaryn-client/Shaders/Shadows/Reprojection.slang"
struct ProbeCase {
    float3 old;
    float3 world;
    float3 eye;
    float3 ray;
    uint voxel;
    float tolerance;
};
StructuredBuffer<ProbeCase> cases;
RWStructuredBuffer<uint> results;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    ProbeCase c=cases[id.x];
    results[id.x]=shadow_history_position(c.old,c.world,c.voxel,c.eye,c.ray,c.tolerance)?1u:0u;
}
