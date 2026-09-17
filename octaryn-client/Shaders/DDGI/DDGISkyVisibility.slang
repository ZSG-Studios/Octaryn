#ifndef OCTARYN_DDGI_SKY_VISIBILITY
#define OCTARYN_DDGI_SKY_VISIBILITY
#include "../RayTracing/WorldSkyVisibility.slang"
#include "DDGIEnvironment.slang"
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
        float cosine=max(dot(normal,L),0);
        if(cosine<=0 && normal.y>=0)continue;
        float transmission=world_sky_transmission(position+normal*.02,L,raySettings.y);
        float3 env=ddgi_diffuse_environment(L,sunDirection,sky)*transmission;
        float skyCos=max(L.y,0);
        skySum+=env*skyCos;skyWeight+=skyCos;
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
