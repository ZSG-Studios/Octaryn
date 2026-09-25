#ifndef OCTARYN_BLOCK_TRANSPORT_LOCAL_SAMPLING
#define OCTARYN_BLOCK_TRANSPORT_LOCAL_SAMPLING
#include "../Lighting/LocalVisibility.slang"
struct GILightNode {float4 boundsMinFlux,boundsMax;uint4 links;};
StructuredBuffer<GILightNode> giLightTree;
uniform uint giLightNodeCount;
float bt_light_importance(GILightNode node,float3 position) {
    float3 delta=max(max(node.boundsMinFlux.xyz-position,position-node.boundsMax.xyz),0);
    return node.boundsMinFlux.w/max(dot(delta,delta),.25);
}
uint bt_sample_light(float3 position,inout uint random,out float probability) {
    uint index=0;probability=1;
    for(uint depth=0;depth<17;++depth) {
        GILightNode node=giLightTree[index];
        if(node.links.w==1)return node.links.z;
        float a=bt_light_importance(giLightTree[node.links.x],position);
        float b=bt_light_importance(giLightTree[node.links.y],position);
        float p=clamp(a+b>0?a/(a+b):.5,1.0/64,63.0/64);
        bool left=bt_random(random)<p;
        probability*=left?p:1-p;index=left?node.links.x:node.links.y;
    }
    probability=0;return 0;
}
float3 bt_local_direct(float3 position,float3 normal,inout uint random,bool certifiedSource=true) {
    if(giLightNodeCount==0)return 0;
    float total=0,selectedWeight=0,selectedDistance=0;
    uint selected=0;float3 selectedDirection=0,selectedValue=0;
    for(uint i=0;i<8;++i) {
        float probability;uint light=bt_sample_light(position,random,probability);
        if(probability<=0)continue;
        float3 direction;float distance;
        float2 sample=float2(bt_random(random),bt_random(random));
        float3 value=local_light_radiance(position,light,sample,direction,distance)*
            saturate(dot(normal,direction))/probability;
        // The entire source segment must stay in the certified resident box.
        if(certifiedSource && !bt_inside(position+direction*distance))value=0;
        float weight=dot(value,float3(.2126,.7152,.0722));total+=weight;
        if(weight>0 && bt_random(random)*total<weight) {
            selected=light;selectedWeight=weight;selectedDistance=distance;
            selectedDirection=direction;selectedValue=value;
        }
    }
    if(selectedWeight<=0 || selectedDistance>raySettings.y)return 0;
    float bias=max(raySettings.z,.001);
    float3 ignored=localLights[selected].axisVType.w==3?floor(localLights[selected].positionRange.xyz):float3(1e30);
    float3 origin=position+normal*bias,direction=selectedDirection;
    float distance=max(selectedDistance-bias,.0001),minimum=-1;
    if(certifiedSource) {
        if(!bt_outward_origin(position,normal,origin))return 0;
        float3 segment=position+selectedDirection*selectedDistance-origin;
        distance=length(segment);if(!isfinite(distance) || distance<=0)return 0;
        direction=segment/distance;minimum=0;
    }
    bt_record_local_ray();
    float visible=world_trace_ray(origin,direction,distance,
        true,ignored,false,true,certifiedSource,minimum).hit?0:1;
    return selectedValue*visible*(total/selectedWeight)/(8*3.14159265359);
}
#endif
