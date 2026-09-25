#ifndef OCTARYN_BLOCK_TRANSPORT_LOCAL_AREA
#define OCTARYN_BLOCK_TRANSPORT_LOCAL_AREA
#include "LocalAreaPosition.slang"
#include "LocalSampling.slang"
float3 bt_local_area_direct(BtSurface surface,uint batch,inout uint random) {
    if(giLightNodeCount==0)return 0;
    float3 normal=bt_normal(surface.key.w),selectedPosition=0;
    float total=0,selectedWeight=0,selectedDistance=0;
    uint selected=0;float3 selectedDirection=0,selectedValue=0;
    for(uint i=0;i<8;++i) {
        float3 position;if(!bt_local_area_position(surface,batch,i,random,position) || !bt_inside(position))continue;
        float probability;uint light=bt_sample_light(position,random,probability);
        if(probability<=0)continue;
        float3 direction;float distance;
        float2 sample=float2(bt_random(random),bt_random(random));
        float3 value=local_light_radiance(position,light,sample,direction,distance)*
            saturate(dot(normal,direction))/probability;
        if(!bt_inside(position+direction*distance))value=0;
        float weight=dot(value,float3(.2126,.7152,.0722));total+=weight;
        if(weight>0 && bt_random(random)*total<weight) {
            selected=light;selectedWeight=weight;selectedDistance=distance;selectedPosition=position;
            selectedDirection=direction;selectedValue=value;
        }
    }
    if(selectedWeight<=0 || selectedDistance>raySettings.y)return 0;
    float3 origin;if(!bt_outward_origin(selectedPosition,normal,origin))return 0;
    float3 segment=selectedPosition+selectedDirection*selectedDistance-origin;
    float distance=length(segment);if(!isfinite(distance) || distance<=0)return 0;
    bt_record_local_ray();
    float3 ignored=localLights[selected].axisVType.w==3?floor(localLights[selected].positionRange.xyz):float3(1e30);
    float visible=world_trace_ray(origin,segment/distance,distance,true,ignored,false,true,true,0).hit?0:1;
    return selectedValue*visible*(total/selectedWeight)/(8*3.14159265359);
}
#endif
