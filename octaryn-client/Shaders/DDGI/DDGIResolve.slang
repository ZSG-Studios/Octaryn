#ifndef OCTARYN_DDGI_RESOLVE
#define OCTARYN_DDGI_RESOLVE
#include "DDGISample.slang"
#include "DDGISkyVisibility.slang"
// Both camera and reflected diffuse receivers use the same uncovered sky term.
float3 ddgi_resolve_fallback(float3 position,float3 normal,float3 sunDirection,
    float4 sky,float3 legacyAmbient,float coverage) {
    if(coverage<.999 && (ddgiGrid.w!=0 || ddgiFineGrid.w!=0))
        return ddgi_visible_sky(position,normal,sunDirection,sky);
    return legacyAmbient;
}
#endif
