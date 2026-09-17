#ifndef OCTARYN_PBREVALUATE_SLANG
#define OCTARYN_PBREVALUATE_SLANG
float3 octaryn_pbr_identity(float3 albedo)
{
    return albedo;
}

// Original shader_common.glsl hemisphere ambient and composite.comp.glsl resolve.
float original_ambient_visibility(float3 normal,bool sprite,float floorValue,float sky) {
    if (sprite) return max(floorValue,0.72);
    float up=saturate(normal.y*0.5+0.5);
    float hemisphere=lerp(0.18,1.0,up*(0.65+up*0.35));
    hemisphere=saturate(hemisphere+pow(1-up,1.35)*pow(1-sky,0.85)*0.18);
    return floorValue+(1-floorValue)*hemisphere;
}
float3 original_ambient_light(float rawVisibility,float sky) {
    // Calibrate daylight radiance for the linear HDR path without lifting night
    // or bypassing the caller's sky visibility / probe occlusion.
    float daylightScale=lerp(1.0,2.5,smoothstep(0.42,1.0,saturate(sky)));
    rawVisibility=saturate(rawVisibility); sky=max(saturate(sky),0.42);
    float localSky=smoothstep(0.0,0.02,rawVisibility);
    float visibility=max(pow(rawVisibility,1.08)*localSky,lerp(0.0,0.34,sky)*localSky);
    float scale=lerp(0.34,1.0,sqrt(sky));
    return lerp(float3(0.090,0.090,0.094),float3(0.235,0.240,0.245),sqrt(sky))*visibility*scale*daylightScale;
}
float3 original_present(float3 hdr) {
    float3 color=max(hdr,0); color=color/(color+1);
    return select(color<=0.0031308,color*12.92,1.055*pow(color,1.0/2.4)-0.055);
}

#endif
