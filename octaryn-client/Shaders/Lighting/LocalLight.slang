#ifndef OCTARYN_LOCAL_LIGHT_SLANG
#define OCTARYN_LOCAL_LIGHT_SLANG
struct LocalLight {float4 positionRange,colorIntensity,directionOuter,axisUInner,axisVType;};
StructuredBuffer<LocalLight> localLights;
// Rectangle parameters are uniform unit-square samples; the area Jacobian is included.
float3 local_light_radiance(float3 surfacePosition,uint lightId,float2 sample,out float3 L,out float distance) {
    LocalLight light=localLights[lightId];float3 position=light.positionRange.xyz;
    uint type=uint(light.axisVType.w);
    if(type==2)position+=(sample.x*2-1)*light.axisUInner.xyz+(sample.y*2-1)*light.axisVType.xyz;
    float3 delta=position-surfacePosition;distance=length(delta);L=delta/max(distance,1e-6);
    if(distance<.001 || distance>=light.positionRange.w)return 0;
    float attenuation=pow(saturate(1-pow(distance/light.positionRange.w,4)),2)/max(distance*distance,.01);
    if(type==1)attenuation*=smoothstep(light.directionOuter.w,light.axisUInner.w,dot(-L,light.directionOuter.xyz));
    if(type==2) {
        float3 area=cross(light.axisUInner.xyz,light.axisVType.xyz);
        attenuation*=4*length(area)*saturate(dot(normalize(light.directionOuter.xyz),-L));
    }
    return light.colorIntensity.rgb*light.colorIntensity.w*attenuation;
}
#endif
