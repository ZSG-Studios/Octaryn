#include "WorldRayQuery.slang"
struct ProbeRay {float4 origin;float4 direction;};
struct ProbeResult {uint4 identity;float4 distanceNormal;float4 position;float4 albedo;};
StructuredBuffer<ProbeRay> probeRays;
RWStructuredBuffer<ProbeResult> probeResults;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    ProbeRay ray=probeRays[id.x];
    WorldRayHit hit=world_trace_ray(ray.origin.xyz,ray.direction.xyz,ray.origin.w);
    ProbeResult result;
    result.identity=uint4(hit.hit?1:0,hit.material,hit.flags,hit.primitive);
    result.distanceNormal=float4(hit.distance,hit.normal);
    result.position=float4(hit.position,0);
    result.albedo=hit.albedo;
    probeResults[id.x]=result;
}
