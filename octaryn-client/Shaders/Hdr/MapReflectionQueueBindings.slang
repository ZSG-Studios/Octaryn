#include "../Lighting/Surface.slang"
#include "../Shadows/Reprojection.slang"
#include "MapReflections.slang"
#include "../RayTracing/MapTrace.slang"
Texture2D<float4> colors,positions,voxels,materials;
Texture2D<float4> emissive;
Texture2D<float4> previousRadiance,previousSurface,previousMaterial;
Texture2D<float> previousPosition;
Texture2D<float2> previousMoments;
RWTexture2D<float4> reflectionHistory,reflectionSurface,reflectionMaterial;
RWTexture2D<float> reflectionPosition;
RWTexture2D<float2> reflectionMoments;
uniform uint referenceMode,historySearch;
uniform uint referenceRecovery,recoveryRayBudget;
uniform uint screenSpaceReflections;
uniform float4 currentRight,currentUp,currentProjection,currentJitter;
uniform float4 eye,previousPositionCamera,previousRight,previousUp,previousForward;
uniform float4 previousProjection,previousJitter,currentForward,sun,lighting,dimensions,sourceDimensions;
#ifdef OCTARYN_REFLECTION_SCREEN_HITS
Texture2D<uint> surfaceFlags;
#include "MapReflectionScreenTrace.slang"
#endif
#include "MapReflectionHistory.slang"
struct ReflectionIntersection {
    uint instance,material,primitive,flags;
    float2 barycentrics;float distance;uint padding;
};
RWStructuredBuffer<uint> reflectionReceivers,reflectionRecovery,reflectionCounts;
RWStructuredBuffer<ReflectionIntersection> reflectionIntersections;
struct RecoverySample {float4 radiance;float2 moments;float2 padding;};
RWStructuredBuffer<RecoverySample> recoverySamples;
RWStructuredBuffer<uint> recoveryHigh,recoveryLow;
uint queue_append(uint counter) {
    uint base=0;
#ifdef OCTARYN_REFLECTION_COHERENT
    uint count=WaveActiveCountBits(true),offset=WavePrefixCountBits(true);
    if(WaveIsFirstLane())InterlockedAdd(reflectionCounts[counter],count,base);
    return WaveReadLaneFirst(base)+offset;
#else
    InterlockedAdd(reflectionCounts[counter],1,base);return base;
#endif
}
uint recovery_phase(uint2 pixel,uint count) {
#ifdef OCTARYN_REFLECTION_COHERENT
    pixel/=8;
#endif
    uint hash=pixel.x*1664525u+pixel.y*1013904223u;
    hash^=hash>>16;hash*=2246822519u;hash^=hash>>13;
    return (uint(dimensions.z)*uint(reflectionSampling.y)+hash)%count;
}
uint2 queue_pixel(uint packed) {uint pixel=packed&0xffffffu;return uint2(pixel%uint(dimensions.x),pixel/uint(dimensions.x));}
uint2 queue_source(uint2 pixel) {
    return min(uint2((float2(pixel)+.5)*sourceDimensions.xy/dimensions.xy),uint2(sourceDimensions.xy)-1);
}
struct ReflectionRay {float3 direction;float3 weight;bool valid;};
ReflectionRay queue_ray(uint2 source,uint sample,uint count) {
    ReflectionRay ray=(ReflectionRay)0;
    float3 position=positions[source].xyz,normal=lighting_normal(lighting_voxel(voxels[source]));
    float3 view=-position/max(length(position),1e-5);
    if(dot(normal,view)<=0 || raySettings.x==0 || reflectionRange<=0)return ray;
    float3 tangent=normalize(cross(abs(normal.y)<.999?float3(0,1,0):float3(1,0,0),normal));
    float3 bitangent=cross(normal,tangent);
    float roughness=saturate(materials[source].r),metallic=saturate(materials[source].g);
    float alpha=max(roughness*roughness,.0025),a2=alpha*alpha;
    float3 localView=float3(dot(tangent,view),dot(bitangent,view),dot(normal,view));
    float2 random=float2((float(sample)+.5)/count,frac(float(sample)*.61803398875));
    float3 sampled=count==1?float3(0,0,1):pbr_visible_normal(localView,alpha,random);
    float3 halfway=normalize(tangent*sampled.x+bitangent*sampled.y+normal*sampled.z);
    float vh=dot(view,halfway);if(vh<=0)return ray;
    ray.direction=reflect(-view,halfway);
    float nl=dot(normal,ray.direction);if(nl<=0)return ray;
    ray.weight=pbr_fresnel(lerp(float3(.04),colors[source].rgb,metallic),vh)*(count==1?1:pbr_smith_g1(nl,a2));
    ray.valid=true;return ray;
}
float3 queue_origin(uint2 source,float3 direction) {
    return positions[source].xyz+eye.xyz+lighting_normal(lighting_voxel(voxels[source]))*.001+direction*.0005;
}
ReflectionIntersection queue_intersection(uint2 source,ReflectionRay ray) {
    ReflectionIntersection record=(ReflectionIntersection)0;
    if(!ray.valid)return record;
    float3 origin=queue_origin(source,ray.direction);
    ray_diagnostic(6);
    WorldRayHit hit=map_trace_intersection(origin,ray.direction,reflectionRange);
    record.instance=hit.instance;record.material=hit.material;record.primitive=hit.primitive;
    record.barycentrics=hit.uv;record.distance=hit.distance;record.flags=hit.hit?3u:2u;
    return record;
}
float3 queue_radiance(WorldRayHit hit,float3 direction,inout bool dynamic) {
    if(!hit.hit)return diffuse_environment(direction,sun.xyz,lighting);
    ray_diagnostic_max(12,mapDynamicInstanceStart);
    ray_diagnostic_max(13,hit.instance&0x7fffffu);
    if(world_ray_dynamic(hit))ray_diagnostic(8);
    dynamic=dynamic || world_ray_dynamic(hit);
    hit=world_map_ray_surface(hit,direction);
    float visibility=0;
    if(sun.w!=0 && dot(hit.normal,sun.xyz)>0) {
        ray_diagnostic(7);
        WorldRayHit blocker=map_trace_intersection(hit.position+hit.normal*.001+sun.xyz*.0005,sun.xyz,raySettings.y,true);
        if(world_ray_dynamic(blocker))ray_diagnostic(9);
        dynamic=dynamic || world_ray_dynamic(blocker);visibility=blocker.hit?0:1;
    }
    return hit.emission+pbr_direct(hit.albedo.rgb,hit.metallic,hit.roughness,hit.normal,-direction,sun.xyz)*sun.w*visibility;
}
float3 queue_intersection_radiance(ReflectionIntersection record,uint2 source,float3 direction,inout bool dynamic) {
#ifdef OCTARYN_REFLECTION_SCREEN_HITS
    if((record.flags&4u)!=0) {
        uint2 pixel=uint2(record.primitive%uint(sourceDimensions.x),record.primitive/uint(sourceDimensions.x));
        float3 normal=lighting_normal(lighting_voxel(voxels[pixel]));float visibility=0;
        if(sun.w!=0 && dot(normal,sun.xyz)>0) {
            ray_diagnostic(7);
            float3 point=queue_origin(source,direction)+direction*record.distance;
            WorldRayHit blocker=map_trace_intersection(point+normal*.001+sun.xyz*.0005,sun.xyz,raySettings.y,true);
            if(world_ray_dynamic(blocker))ray_diagnostic(9);
            dynamic=dynamic || world_ray_dynamic(blocker);visibility=blocker.hit?0:1;
        }
        return emissive[pixel].rgb+pbr_direct(colors[pixel].rgb,materials[pixel].g,materials[pixel].r,
            normal,-direction,sun.xyz)*sun.w*visibility;
    }
#endif
    WorldRayHit hit=(WorldRayHit)0;hit.hit=(record.flags&1u)!=0;
    hit.instance=record.instance;hit.material=record.material;hit.primitive=record.primitive;
    hit.uv=record.barycentrics;hit.distance=record.distance;
    hit.position=queue_origin(source,direction)+direction*record.distance;
    return queue_radiance(hit,direction,dynamic);
}
void queue_resolve(uint2 pixel,float3 sample,bool inherited,bool dynamic) {
    float4 history=reflectionHistory[pixel];float2 old=reflectionMoments[pixel];
    float weight=inherited && !dynamic?(history.a-1)/max(history.a,1):0;
    float luminance=dot(sample,float3(.2126,.7152,.0722));float bounded=luminance/(1+luminance);
    reflectionHistory[pixel]=float4(lerp(sample,history.rgb,weight),dynamic?-1:history.a);
    if(dynamic)ray_diagnostic(11);
    reflectionMoments[pixel]=lerp(float2(bounded,bounded*bounded),old,weight);
}
