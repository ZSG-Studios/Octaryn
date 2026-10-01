RaytracingAccelerationStructure scene;
StructuredBuffer<uint4> batches;
StructuredBuffer<uint4> triangles;
RWStructuredBuffer<uint4> results;
uniform uint probeCount;
[shader("compute")]
[numthreads(32,1,1)]
void probeRays(uint lane:SV_DispatchThreadID) {
  if(lane>probeCount)return;
  RayDesc ray;ray.Origin=float3(lane*4,0,0);ray.Direction=float3(0,0,1);ray.TMin=.01;ray.TMax=10;
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
    query.TraceRayInline(scene,RAY_FLAG_NONE,255,ray);
    bool candidate=false;
    while(query.Proceed())if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE) {
        candidate=true;query.CommitNonOpaqueTriangleHit();
    }
  if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
    uint4 record=triangles[batches[query.CommittedInstanceID()].x+query.CommittedPrimitiveIndex()];
    results[lane]=uint4(record.x,record.y|(uint(candidate)<<31),record.z,asuint(query.CommittedRayT()));
  } else results[lane]=uint4(0xffffffff);
}
