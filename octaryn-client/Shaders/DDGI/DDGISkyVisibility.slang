#ifndef OCTARYN_DDGI_SKY_VISIBILITY
#define OCTARYN_DDGI_SKY_VISIBILITY
#include "../RayTracing/WorldRayQuery.slang"
#include "DDGIEnvironment.slang"
// Stable angular quadrature for surfaces outside the irradiance cache. This is
// direct sky irradiance, not unoccluded ambient or a second indirect bounce.
float3 ddgi_visible_sky(float3 position,float3 normal,float3 sunDirection,float4 sky) {
    const float3 directions[4]={normalize(float3(1,1,1)),normalize(float3(-1,1,1)),
        normalize(float3(1,1,-1)),normalize(float3(-1,1,-1))};
    float3 sum=0;float weight=0;
    [unroll]for(uint i=0;i<4;++i) {
        float cosine=max(dot(normal,directions[i]),0);
        if(cosine<=0)continue;
        weight+=cosine;
        if(!world_trace_ray(position+normal*.02,directions[i],raySettings.y,true).hit)
            sum+=ddgi_diffuse_environment(directions[i],sunDirection,sky)*cosine;
    }
    // The upper hemisphere covers half a vertical receiver's cosine lobe.
    return sum/max(weight,1e-6)*saturate((1+normal.y)*.5);
}
#endif
