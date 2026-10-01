#include "MapReflectionQueueBindings.slang"
#include "MapReflectionAdmission.slang"
bool recovery_neighbor(uint2 center,int2 tap) {
    if(any(tap<0) || any(tap>=int2(dimensions.xy)) || reflectionHistory[tap].a!=1)return false;
    float depth=reflectionPosition[center],otherDepth=reflectionPosition[tap];
    float4 surface=reflectionSurface[center],otherSurface=reflectionSurface[tap];
    float4 material=reflectionMaterial[center],otherMaterial=reflectionMaterial[tap];
    return otherDepth>0 && abs(otherDepth-depth)<=max(.025,depth*.002) &&
        dot(surface.xyz,otherSurface.xyz)>=.98 && abs(surface.w-otherSurface.w)<=.03 &&
        distance(material.rgb,otherMaterial.rgb)<=.035 && abs(material.a-otherMaterial.a)<=.035;
}
[shader("compute")]
[numthreads(64,1,1)]
void recovery_classify_main(uint3 id:SV_DispatchThreadID) {
    id.x+=id.y*4194240u;
    if(id.x>=reflectionCounts[1] || referenceRecovery!=0)return;
    uint packed=reflectionRecovery[id.x];uint2 pixel=queue_pixel(packed);
    RecoverySample center=recoverySamples[packed];
    if(center.radiance.a<2 || uint(reflectionSampling.x)<=2)return;
    float2 moments=center.moments;uint neighbors=1;
    float contrast=0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        if(x==0 && y==0)continue;
        int2 tap=int2(pixel)+int2(x,y);
        if(!recovery_neighbor(pixel,tap))continue;
        RecoverySample other=recoverySamples[uint(tap.y)*uint(dimensions.x)+uint(tap.x)];
        moments+=other.moments;++neighbors;
        contrast=max(contrast,abs(other.moments.x-center.moments.x));
    }
    moments/=float(neighbors);
    float variance=max(0,moments.y-moments.x*moments.x)/max(.01,moments.x*moments.x);
    bool high=neighbors<3 || variance>.2 || contrast>.25 || reflectionSurface[pixel].w<.25;
    uint index;
    if(high) {index=queue_append(2);recoveryHigh[index]=packed;}
    else if(variance>.05 || contrast>.08) {index=queue_append(3);recoveryLow[index]=packed;}
}
[shader("compute")]
[numthreads(64,1,1)]
void recovery_refine_main(uint3 id:SV_DispatchThreadID) {
    id.x+=id.y*4194240u;
    uint2 admitted=reflection_recovery_admission(reflectionCounts[2],reflectionCounts[3],recoveryRayBudget);
    if(id.x>=admitted.x+admitted.y)return;
    // Admit the high-variance bucket first. The total added work cannot exceed
    // the CPU's measured-budget cap, even after a full-screen camera cut.
    bool high=id.x<admitted.x;uint index=high?id.x:id.x-admitted.x;
    uint packed=high?recoveryHigh[index]:recoveryLow[index];
    uint2 pixel=queue_pixel(packed),source=queue_source(pixel);
    RecoverySample initial=recoverySamples[packed];
    bool dynamic=initial.padding.x!=0;
    uint count=map_reflection_sample_count(saturate(materials[source].r));
    uint first=uint(initial.radiance.a),extra=min(count-first,high?6u:2u);
    float3 sum=initial.radiance.rgb*float(first);float2 moments=initial.moments*float(first);
    uint phase=recovery_phase(pixel,count);
    for(uint i=0;i<extra;++i) {
        ReflectionRay ray=queue_ray(source,(phase+first+i)%count,count);float3 sample=0;
        if(ray.valid) {
            ReflectionIntersection record=queue_intersection(source,ray);
            sample=queue_intersection_radiance(record,source,ray.direction,dynamic)*ray.weight;
        }
        float luminance=dot(sample,float3(.2126,.7152,.0722));float bounded=luminance/(1+luminance);
        sum+=sample;moments+=float2(bounded,bounded*bounded);
    }
    queue_resolve(pixel,sum/float(first+extra),false,dynamic);
    reflectionMoments[pixel]=moments/float(first+extra);
}
