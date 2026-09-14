#ifndef OCTARYN_DDGI_ENVIRONMENT
#define OCTARYN_DDGI_ENVIRONMENT
#include "../Materials/PbrEvaluate.slang"
// Linear diffuse radiance, separate from the stylized display sky and sun disk.
// Match the existing unoccluded ambient energy. Probe integration stores pi*L;
// the Lambertian resolve divides by pi, so no extra pi or inverse tone map belongs here.
float3 ddgi_diffuse_environment(float3 direction,float3 sunDirection,float4 skyState) {
    float3 baseline=original_ambient_light(1,skyState.x)*max(skyState.y,0);
    float zenith=saturate(direction.y);
    float day=smoothstep(-.08,.20,sunDirection.y)*saturate(skyState.x);
    float3 tint=lerp(float3(.94,.99,1.08),float3(1),day);
    tint*=lerp(float3(1),float3(.92,.99,1.08),zenith*day);
    // Broad, restrained twilight tint; direct solar radiance is traced separately.
    float horizon=1-saturate(abs(direction.y));
    tint=lerp(tint,float3(1.08,.98,.90),saturate(skyState.z)*horizon*horizon);
    const float3 luminance=float3(.2126,.7152,.0722);
    float3 radiance=baseline*tint;
    return radiance*(dot(baseline,luminance)/max(dot(radiance,luminance),1e-8));
}
#endif
