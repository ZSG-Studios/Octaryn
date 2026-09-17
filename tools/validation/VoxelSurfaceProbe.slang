#include "../../octaryn-client/Shaders/VoxelTracing/VoxelSurfaceQuery.slang"
StructuredBuffer<VoxelTraceRay> probeRays;
RWStructuredBuffer<VoxelSurfaceHit> probeHits;
uniform VoxelTraceBounds probeBounds;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    probeHits[id.x]=voxel_trace_surface(probeRays[id.x],probeBounds);
}
