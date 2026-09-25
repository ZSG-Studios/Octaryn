#include "WorldRayQuery.slang"
#include "WorldSkyVisibility.slang"
#include "../Lighting/LocalVisibility.slang"
struct ProbeRay {float4 origin;float4 direction;};
struct ProbeResult {uint4 identity;float4 distanceNormal;float4 position;float4 albedo;float4 visibility;};
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
    result.visibility=float4(world_trace_ray(ray.origin.xyz,ray.direction.xyz,ray.origin.w,true).hit?0:1,
        world_ray_visibility(ray.origin.xyz,ray.direction.xyz,ray.direction.xyz),
        world_local_light_visibility(ray.origin.xyz,ray.direction.xyz,id.x,ray.direction.xyz,ray.origin.w),
        world_sky_transmission(ray.origin.xyz,ray.direction.xyz,ray.origin.w));
    probeResults[id.x]=result;
}
