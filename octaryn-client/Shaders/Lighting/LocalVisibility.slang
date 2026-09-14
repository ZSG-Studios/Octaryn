#ifndef OCTARYN_LOCAL_VISIBILITY_SLANG
#define OCTARYN_LOCAL_VISIBILITY_SLANG
#include "../RayTracing/WorldRayQuery.slang"
#include "LocalLight.slang"
float world_local_light_visibility(float3 position,float3 normal,uint light,float3 direction,float distance) {
    if(distance>raySettings.y)return 0;
    float bias=max(raySettings.z,.001);
    float3 origin=position+normal*(dot(normal,direction)>=0?bias:-bias);
    // Exclude only the source voxel; immediately adjacent walls still occlude.
    float3 ignored=localLights[light].axisVType.w==3?floor(localLights[light].positionRange.xyz):float3(1e30);
    return world_trace_ray(origin,direction,max(distance-bias,.0001),true,ignored).hit?0:1;
}
#endif
