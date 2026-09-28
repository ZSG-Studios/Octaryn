RaytracingAccelerationStructure scene;
StructuredBuffer<uint4> batches;
StructuredBuffer<uint4> triangles;
RWStructuredBuffer<uint4> results;
[shader("compute")]
[numthreads(32,1,1)]
void probeRays(uint lane:SV_DispatchThreadID) {
  if(lane>=18)return;
  RayDesc ray;ray.Origin=float3(lane*4,0,0);ray.Direction=float3(0,0,1);ray.TMin=.01;ray.TMax=10;
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
  query.TraceRayInline(scene,RAY_FLAG_NONE,255,ray);
  while(query.Proceed())if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE)query.CommitNonOpaqueTriangleHit();
  if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
    uint4 record=triangles[batches[query.CommittedInstanceID()].x+query.CommittedPrimitiveIndex()];
    results[lane]=uint4(record.x,record.y,record.z,asuint(query.CommittedRayT()));
  } else results[lane]=uint4(0xffffffff);
}
