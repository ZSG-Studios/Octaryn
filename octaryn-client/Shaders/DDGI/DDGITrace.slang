#include "../RayTracing/WorldRayQuery.slang"
#include "DDGISample.slang"
#include "../Materials/LabPbr.slang"
#include "../Lighting/LocalLight.slang"
#include "../Lighting/LocalVisibility.slang"
#include "../Lighting/Reservoir.slang"
#include "DDGIEnvironment.slang"
StructuredBuffer<uint> ddgiSelection;
RWStructuredBuffer<DDGIRay> ddgiRays;
uniform float4 ddgiSun;
uniform float4 ddgiSky; // sky visibility, ambient intensity, twilight, celestial
uniform uint ddgiUpdateCount;
uniform uint ddgiLightCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint total=ddgiUpdateCount*ddgiFrame.y;if(id.x>=total)return;
    uint slot=id.x/ddgiFrame.y,rayIndex=id.x%ddgiFrame.y,probeIndex=ddgiSelection[slot];
    DDGIControl control=ddgiControls[probeIndex];DDGIProbe probe=ddgiProbes[probeIndex];
    bool reset=control.version!=probe.metadata.x;
    bool changed=control.refreshFrame>probe.metadata.y;
    DDGIRay ray=(DDGIRay)0;
    uint elapsed=ddgiFrame.x-probe.metadata.y;
    if(!reset&&!changed&&((probe.offset.w==2&&elapsed<120)||(probe.offset.w==1&&elapsed<60))) {
        ray.radianceDistance.w=-2*ddgiParameters.z;ddgiRays[id.x]=ray;return;
    }
    float3 origin=float3(control.cell)*ddgiParameters.x+(reset?float3(0):probe.offset.xyz);
    float3 direction=ddgi_direction(rayIndex,ddgiFrame.y,ddgiFrame.x,probeIndex);
    WorldRayHit hit=world_trace_ray(origin,direction,ddgiParameters.z);
    // Two-sided vegetation is not evidence that a probe is inside a solid voxel.
    bool backFace=hit.hit&&hit.backFace&&(hit.flags&1)!=0;
    if(rayIndex<ddgi_fixed_rays()) {
        ray.radianceDistance.w=hit.hit?(backFace?-hit.distance:hit.distance):ddgiParameters.z;
        ray.directionHit=float4(direction,hit.hit?1:0);ddgiRays[id.x]=ray;return;
    }
    float3 radiance=0;
    if(!hit.hit) {
        radiance=ddgi_diffuse_environment(direction,ddgiSun.xyz,ddgiSky);
    } else if(!backFace) {
        float4 specular=world_ray_sample(atlasSpecular,hit.uv,hit.voxel);
        float metallic=decode_labpbr_metallic(specular);
        float direct=max(dot(hit.normal,ddgiSun.xyz),0)*max(ddgiSun.w,0)/3.14159265359;
        if(direct>0)direct*=world_ray_visibility(hit.position,hit.normal,ddgiSun.xyz);
        float coverage;float3 bounced=ddgi_sample(hit.position,hit.normal,coverage)*coverage/3.14159265359;
        float3 local=0;
        if(ddgiLightCount>0) {
            uint rng=restir_hash(id.x+ddgiFrame.x*9781+1);
            uint light=min(uint(restir_random(rng)*ddgiLightCount),ddgiLightCount-1);
            float2 sample=float2(restir_random(rng),restir_random(rng));float3 L;float distance;
            float3 incident=local_light_radiance(hit.position,light,sample,L,distance);
            local=incident*max(dot(hit.normal,L),0)*ddgiLightCount/3.14159265359;
            if(any(local>0))local*=world_local_light_visibility(hit.position,hit.normal,light,L,distance);
        }
        // Direct lighting at the ray hit is one bounce at the camera surface.
        radiance=world_ray_emission(hit)+hit.albedo.rgb*(1-metallic)*(direct+local+bounced);
    }
    ray.radianceDistance=float4(clamp(radiance,0,64),hit.hit?(backFace?-hit.distance:hit.distance):ddgiParameters.z);
    ray.directionHit=float4(direction,hit.hit?1:0);ddgiRays[id.x]=ray;
}
