#ifndef OCTARYN_SRC_HIT_RADIANCE
#define OCTARYN_SRC_HIT_RADIANCE
#include "Geometry.slang"
#include "Sample.slang"
#include "../Materials/LabPbr.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../Lighting/LocalLight.slang"
uniform float4 srcSun;
uniform float4 srcSky; // visibility, ambient intensity, twilight, celestial
uniform uint srcLightCount;
float3 src_feedback(float3 position,float3 normal) {
    uint lod=src_lod(position)+2,probe=src_find(src_cell(position,lod,0),lod,0);
    if(probe==SRC_INVALID)return 0;
    if(!src_visible(src_surface_start(position,normal),src_probe_surface(probe,0)))return 0;
    // Previous-frame probe irradiance bin at this normal; oct stores E, and the
    // cosine-weighted radiance mean used by bounce terms is E/pi.
    float4 v=src_oct_sample_prev(probe,normal);
    return v.w>0?v.xyz/SRC_PI:float3(0,0,0);
}
float3 src_environment(float3 direction) {
    float3 baseline=original_ambient_light(1,srcSky.x)*max(srcSky.y,0);
    float day=smoothstep(-.08,.20,srcSun.y)*saturate(srcSky.x),zenith=saturate(direction.y);
    float3 tint=lerp(float3(.94,.99,1.08),float3(1),day)*lerp(float3(1),float3(.92,.99,1.08),zenith*day);
    float horizon=1-saturate(abs(direction.y));tint=lerp(tint,float3(1.08,.98,.90),saturate(srcSky.z)*horizon*horizon);
    float3 radiance=baseline*tint;float3 luminance=float3(.2126,.7152,.0722);
    return radiance*(dot(baseline,luminance)/max(dot(radiance,luminance),1e-8));
}
float3 src_hit_radiance(SrcHit hit,inout uint rng,uint shadowSteps) {
    VoxelSurfaceHit s=hit.surface;
    if(s.backFace&&(s.flags&1)!=0)return 0;
    float4 specular=atlasSpecular.SampleLevel(atlasCutout,float3(s.uv,get_index(s.packedVoxel)),0);
    float metallic=decode_labpbr_metallic(specular);
    float e=specular.a>=1?0:saturate(specular.a*(255.0/254.0));
    float3 emission=s.albedo.rgb*max(float3(e),min(blockMaterials[s.cell.material].emission.rgb,float3(8)));
    if((s.flags&512)!=0)emission=max(emission,s.albedo.rgb+float3(1,.22,.02)*.58);
    float3 start=src_surface_start(hit.position,s.normal);
    // Shadow traces stop cheaply at the resident/unknown boundary (UNKNOWN is
    // conservatively occluded) and carry a worst-case step budget so dense
    // resident geometry cannot stall a frame; EXHAUSTED stays occluded too.
    float sun=max(dot(s.normal,srcSun.xyz),0)*max(srcSun.w,0)/SRC_PI;
    if(sun>0 && src_trace_budget(start,srcSun.xyz,0,srcTemporal.z,shadowSteps).status!=VOXEL_TRACE_MISS)sun=0;
    float3 local=0;
    if(srcLightCount>0) {
        uint light=min(uint(src_random(rng)*srcLightCount),srcLightCount-1);float3 direction;float distance;
        float2 sample=float2(src_random(rng),src_random(rng));
        float3 incoming=local_light_radiance(float3(srcOrigin.xyz)+hit.position,light,sample,direction,distance);
        local=incoming*max(dot(s.normal,direction),0)*(float(srcLightCount)/SRC_PI);
        if(any(local>0) && src_trace_budget(start,direction,0,distance,shadowSteps).status!=VOXEL_TRACE_MISS)local=0;
    }
    float3 bounce=src_feedback(hit.position,s.normal)*srcTemporal.y;
    return clamp(emission+s.albedo.rgb*(1-metallic)*(sun+local+bounce),0,64);
}
#endif
