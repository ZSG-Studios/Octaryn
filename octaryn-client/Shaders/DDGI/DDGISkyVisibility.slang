#ifndef OCTARYN_DDGI_SKY_VISIBILITY
#define OCTARYN_DDGI_SKY_VISIBILITY
#include "../RayTracing/WorldRayQuery.slang"
#include "DDGIEnvironment.slang"
// Foliage cubes and cutout cards occlude only part of the sky. Solid cave rock
// still reports a full hit, so enclosed surfaces stay dark.
float ddgi_sky_transmission(WorldRayHit hit) {
    if(!hit.hit)return 1;
    bool foliage=(hit.flags&256)!=0||((hit.flags&2)!=0&&(hit.flags&16)==0);
    return foliage?lerp(.35,.1,saturate(hit.albedo.a)):0;
}
// Stable angular quadrature for surfaces outside the irradiance cache. This is
// direct sky irradiance, not unoccluded ambient or a second indirect bounce.
float3 ddgi_visible_sky(float3 position,float3 normal,float3 sunDirection,float4 sky) {
    const float3 directions[8]={normalize(float3(1,1,1)),normalize(float3(-1,1,1)),
        normalize(float3(1,1,-1)),normalize(float3(-1,1,-1)),
        normalize(float3(1,.22,1)),normalize(float3(-1,.22,1)),
        normalize(float3(1,.22,-1)),normalize(float3(-1,.22,-1))};
    float3 skySum=0,surfaceSum=0;float skyWeight=0,surfaceWeight=0;
    [unroll]for(uint i=0;i<8;++i) {
        float3 L=directions[i];
        WorldRayHit hit=world_trace_ray(position+normal*.02,L,raySettings.y,true);
        float3 env=ddgi_diffuse_environment(L,sunDirection,sky)*ddgi_sky_transmission(hit);
        float skyCos=max(L.y,0);
        skySum+=env*skyCos;skyWeight+=skyCos;
        float cosine=max(dot(normal,L),0);
        if(cosine<=0)continue;
        surfaceSum+=env*cosine;surfaceWeight+=cosine;
    }
    float3 incoming=skySum/max(skyWeight,1e-6);
    float3 direct=surfaceWeight>1e-6?surfaceSum/surfaceWeight:0;
    // Downward leaf faces cannot see the sky; they keep a dim bounce of whatever
    // still reaches the canopy. Enclosed rock has incoming=0, so caves stay dark.
    return direct*saturate((1+normal.y)*.5)+incoming*saturate(-normal.y)*.28;
}
#endif
