#ifndef OCTARYN_MAP_REFLECTIONS
#define OCTARYN_MAP_REFLECTIONS
#include "../Materials/PbrEnvironment.slang"
#include "../RayTracing/WorldRayQuery.slang"
#include "../RayTracing/Diagnostics.slang"
#ifndef OCTARYN_MAP_DEFERRED_MATERIAL
#include "../BlockTransportGI/Reflection.slang"
#endif
#include "../Lighting/DiffuseEnvironment.slang"
#include "../Sky/Fog.slang"
uniform float reflectionRange;
uniform float4 reflectionSampling; // quadrature directions, fresh samples, history frames, reserved
uint map_reflection_sample_count(float roughness) {
    return roughness<.08?1u:uint(reflectionSampling.x);
}
#ifdef OCTARYN_MAP_DEFERRED_MATERIAL
#include "MapReflectionDeferred.slang"
#else
float3 map_reflected_radiance(WorldRayHit hit,float3 view,float4 sun,float4 sky,inout bool dynamic) {
    if(!hit.hit)return 0;
    ray_diagnostic(17);
    ray_diagnostic_max(12,mapDynamicInstanceStart);
    ray_diagnostic_max(13,hit.instance&0x7fffffu);
    if(world_ray_dynamic(hit))ray_diagnostic(8);
    dynamic=dynamic || world_ray_dynamic(hit);
    float metallic=world_ray_metallic(hit),roughness=world_ray_roughness(hit);
    float3 diffuse=0;bool cachedDiffuse=false;
    if(metallic<1) {
      if(btLookupInfo.x!=0)cachedDiffuse=sample_block_transport_reflected_incident(hit.surfaceKey,hit.backFace,diffuse);
    }
    // Roughness does not remove shadowing: an untested secondary hit must not
    // receive direct sun through an occluder.
    if(sun.w!=0 && dot(hit.normal,sun.xyz)>0)ray_diagnostic(7);
    float visibility=0;
    if(sun.w!=0 && dot(hit.normal,sun.xyz)>0) {
        float3 direction=normalize(sun.xyz);
        WorldRayHit blocker=world_trace_ray(hit.position+hit.normal*.001+direction*.0005,direction,raySettings.y,true);
        if(world_ray_dynamic(blocker))ray_diagnostic(9);
        dynamic=dynamic || world_ray_dynamic(blocker);visibility=blocker.hit?0:1;
    }
    if(visibility==0)ray_diagnostic(18);
    float3 fresnel=pbr_environment_reflectance(lerp(float3(.04),hit.albedo.rgb,metallic),roughness,dot(hit.normal,view));
    return world_ray_emission(hit)+hit.albedo.rgb*(1-metallic)*(1-fresnel)*diffuse+
        bt_reflected_sun_brdf(cachedDiffuse,hit.albedo.rgb,metallic,roughness,hit.normal,view,sun.xyz)*sun.w*visibility;
}
#endif
float3 map_reflection_sample(float3 position,float3 normal,float3 view,float3 albedo,
    float metallic,float roughness,float4 sun,float4 sky,uint sample,uint count,inout bool dynamic) {
    if(raySettings.x==0 || reflectionRange<=0 || dot(normal,view)<=0)return 0;
    float3 tangent=normalize(cross(abs(normal.y)<.999?float3(0,1,0):float3(1,0,0),normal));
    float3 bitangent=cross(normal,tangent);
    float alpha=max(roughness*roughness,.0025),a2=alpha*alpha;
    float3 localView=float3(dot(tangent,view),dot(bitangent,view),dot(normal,view));
    float3 f0=lerp(float3(.04),albedo,metallic);
    // Fixed quadrature preserves the reference sample directions across frames.
    float2 random=float2((float(sample)+.5)/count,frac(float(sample)*.61803398875));
    float3 sampled=count==1?float3(0,0,1):pbr_visible_normal(localView,alpha,random);
    float3 halfway=normalize(tangent*sampled.x+bitangent*sampled.y+normal*sampled.z);
    float vh=dot(view,halfway);
    if(vh<=0)return 0;
    float3 direction=reflect(-view,halfway);
    float nl=dot(normal,direction);
    if(nl<=0)return 0;
    ray_diagnostic(6);
    WorldRayHit hit=world_trace_ray(position+normal*.001+direction*.0005,direction,reflectionRange);
    // Reflections and diffuse GI share incident linear radiance. The visible
    // sky is display-authored; inverse tone mapping amplifies saturated blue.
    float3 radiance;
    if(hit.hit)radiance=map_reflected_radiance(hit,-direction,sun,sky,dynamic);
    else radiance=diffuse_environment(direction,sun.xyz,sky);
    // Visible-normal PDF cancels G1(V), D and 4(V.H). Weight stays bounded.
    float weight=count==1?1:pbr_smith_g1(nl,a2);
    return radiance*pbr_fresnel(f0,vh)*weight;
}
float3 map_reflections(float3 position,float3 normal,float3 view,float3 albedo,
    float metallic,float roughness,float4 sun,float4 sky) {
    float3 sum=0;
    bool dynamic=false;
    uint count=map_reflection_sample_count(roughness);
    for(uint sample=0;sample<count;++sample)
        sum+=map_reflection_sample(position,normal,view,albedo,metallic,roughness,sun,sky,sample,count,dynamic);
    return sum/count;
}
#endif
