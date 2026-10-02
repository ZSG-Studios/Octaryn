#include "MapReflectionQueueBindings.slang"
[shader("compute")]
[numthreads(8,8,1)]
void classify_main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(dimensions.xy)))return;
    uint2 source=queue_source(id.xy);float4 position=positions[source];uint voxel=lighting_voxel(voxels[source]);
    if(position.w<=0 || !lighting_is_map(voxel) || lighting_is_unlit(voxel,materials[source])) {
        reflectionHistory[id.xy]=0;reflectionPosition[id.xy]=0;reflectionSurface[id.xy]=0;
        reflectionMaterial[id.xy]=0;reflectionMoments[id.xy]=0;return;
    }
    ray_diagnostic(4);
    float3 normal=lighting_normal(voxel),world=position.xyz+eye.xyz,albedo=colors[source].rgb;
    float roughness=saturate(materials[source].r),metallic=saturate(materials[source].g);
    int2 oldPixel;bool valid=false;
    if(dimensions.w!=0)valid=reflection_history_pixel(world,normal,voxel,roughness,metallic,albedo,length(position.xyz),oldPixel);
    float4 history=float4(0,0,0,1);float2 moments=0;
    uint pixel=id.y*uint(dimensions.x)+id.x,index;
    if(valid) {
        history=previousRadiance[oldPixel];moments=previousMoments[oldPixel];
        float limit=lerp(4,reflectionSampling.z,smoothstep(.08,.5,roughness));
        if(distance(eye.xyz,previousPositionCamera.xyz)>1e-5)limit=min(limit,8);
        history.a=min(history.a+1,limit);
        float variance=max(0,moments.y-moments.x*moments.x)/max(.01,moments.x*moments.x);
        uint count=map_reflection_sample_count(roughness);
        uint fresh=roughness>=.25 && history.a>=8 && variance<.1?1u:min(count,2u);
        index=queue_append(0);reflectionReceivers[index]=pixel|(fresh<<24);
    } else {
        ray_diagnostic(5);index=queue_append(1);reflectionRecovery[index]=pixel;
    }
    reflectionHistory[id.xy]=history;reflectionMoments[id.xy]=moments;
    reflectionPosition[id.xy]=dot(position.xyz,currentForward.xyz);
    reflectionSurface[id.xy]=float4(normal,roughness);reflectionMaterial[id.xy]=float4(albedo,metallic);
}
[shader("compute")]
[numthreads(64,1,1)]
void screen_main(uint3 id:SV_DispatchThreadID) {
#ifdef OCTARYN_REFLECTION_SCREEN_HITS
    id.x+=id.y*4194240u;if(id.x>=reflectionCounts[0])return;
    uint packed=reflectionReceivers[id.x];uint2 source=queue_source(queue_pixel(packed));
    uint count=map_reflection_sample_count(saturate(materials[source].r));
    for(uint i=0;i<(packed>>24);++i) {
        ReflectionIntersection record=(ReflectionIntersection)0;
        ReflectionRay ray=queue_ray(source,(uint(dimensions.z)+i)%count,count);
        if(ray.valid) {
            ray_diagnostic(14);uint2 pixel;float distance;
            bool found=false;
            if(surfaceFlags[source]==1u)found=reflection_screen_hit(queue_origin(source,ray.direction),ray.direction,pixel,distance);
            if(found) {
                record.flags=6;record.distance=distance;
                record.primitive=pixel.y*uint(sourceDimensions.x)+pixel.x;ray_diagnostic(15);
            } else ray_diagnostic(16);
        }
        reflectionIntersections[id.x*2+i]=record;
    }
#endif
}
[shader("compute")]
[numthreads(64,1,1)]
void intersect_main(uint3 id:SV_DispatchThreadID) {
    id.x+=id.y*4194240u;
    if(id.x>=reflectionCounts[0])return;
    uint packed=reflectionReceivers[id.x];uint2 source=queue_source(queue_pixel(packed));
    uint count=map_reflection_sample_count(saturate(materials[source].r));
    for(uint i=0;i<(packed>>24);++i) {
#ifdef OCTARYN_REFLECTION_SCREEN_HITS
        if((reflectionIntersections[id.x*2+i].flags&4u)!=0)continue;
#endif
        ReflectionRay ray=queue_ray(source,(uint(dimensions.z)+i)%count,count);
        reflectionIntersections[id.x*2+i]=queue_intersection(source,ray);
    }
}
[shader("compute")]
[numthreads(64,1,1)]
void shade_main(uint3 id:SV_DispatchThreadID) {
    id.x+=id.y*4194240u;
    if(id.x>=reflectionCounts[0])return;
    uint packed=reflectionReceivers[id.x];uint2 pixel=queue_pixel(packed),source=queue_source(pixel);
    uint count=map_reflection_sample_count(saturate(materials[source].r));float3 sum=0;bool dynamic=false;
    for(uint i=0;i<(packed>>24);++i) {
        ReflectionIntersection record=reflectionIntersections[id.x*2+i];if((record.flags&2u)==0)continue;
        ReflectionRay ray=queue_ray(source,(uint(dimensions.z)+i)%count,count);
        sum+=queue_intersection_radiance(record,source,ray.direction,dynamic)*ray.weight;
    }
    queue_resolve(pixel,sum/float(packed>>24),true,dynamic);
}
[shader("compute")]
[numthreads(64,1,1)]
void recovery_main(uint3 id:SV_DispatchThreadID) {
    id.x+=id.y*4194240u;
    if(id.x>=reflectionCounts[1])return;
    uint2 pixel=queue_pixel(reflectionRecovery[id.x]),source=queue_source(pixel);
    uint count=map_reflection_sample_count(saturate(materials[source].r));float3 sum=0;float2 moments=0;bool dynamic=false;
#ifdef OCTARYN_REFLECTION_BOUNDED_BASE
    uint fresh=min(count,2u),phase=recovery_phase(pixel,count);
    [unroll]
    for(uint i=0;i<2u;++i) {
        if(i>=fresh)continue;
#else
    uint fresh=referenceRecovery!=0?count:min(count,2u);
    uint phase=referenceRecovery!=0?uint(dimensions.z)*uint(reflectionSampling.y):recovery_phase(pixel,count);
    for(uint i=0;i<fresh;++i) {
#endif
        ReflectionRay ray=queue_ray(source,(phase+i)%count,count);float3 sample=0;
        if(ray.valid) {
            ReflectionIntersection record=queue_intersection(source,ray);
            sample=queue_intersection_radiance(record,source,ray.direction,dynamic)*ray.weight;
        }
        float luminance=dot(sample,float3(.2126,.7152,.0722));float bounded=luminance/(1+luminance);
        sum+=sample;moments+=float2(bounded,bounded*bounded);
    }
    RecoverySample result;result.radiance=float4(sum/float(fresh),float(fresh));
    result.moments=moments/float(fresh);result.padding=float2(dynamic?1:0,0);
    recoverySamples[pixel.y*uint(dimensions.x)+pixel.x]=result;
    queue_resolve(pixel,result.radiance.rgb,false,dynamic);
    reflectionMoments[pixel]=result.moments;
}
