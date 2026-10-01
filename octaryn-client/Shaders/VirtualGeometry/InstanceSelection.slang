// InstanceSelection.h owns the 192-byte scalar/std430 contract.
struct InstanceSelectionView {
    float4 eyeFocal,errorScale;
    float4 world[3];
    float4 planes[6];
    uint4 flags;
};
StructuredBuffer<InstanceSelectionView> geometryInstances;
uniform uint geometryInstanceCount;
static const float selectionMaximum=3.402823466e+38;
static const float selectionEpsilon=1.192092896e-7;
float selection_upper(float value) {
    if(!isfinite(value) || value>=selectionMaximum)return selectionMaximum;
    return value==0?0:asfloat(min(asuint(max(0.0,value))+4u,0x7f7fffffu));
}
float selection_length(float3 value) {
    float scale=max(abs(value.x),max(abs(value.y),abs(value.z)));
    if(!isfinite(scale))return selectionMaximum;
    return scale==0?0:selection_upper(scale*length(value/scale));
}
void instance_sphere(uint index,float3 center,float radius,out float3 position,out float extent) {
    float3 magnitude;
    for(uint row=0;row<3;++row) {
        float4 transform=geometryInstances[index].world[row];
        precise float value=dot(transform.xyz,center)+transform.w;
        position[row]=value;
        magnitude[row]=dot(abs(transform.xyz),abs(center))+abs(transform.w);
    }
    // Covers FP32 transform cancellation and the reference sphere's padding.
    float padding=selection_upper(selection_length(magnitude)*(16*selectionEpsilon));
    extent=selection_upper(selection_upper(radius*geometryInstances[index].errorScale.y)+padding);
}
bool instance_visible(uint index,float3 center,float radius) {
    if(radius==selectionMaximum || !all(isfinite(center)))return true;
    if(geometryInstances[index].flags.x!=0)for(uint i=0;i<6;++i) {
        float4 plane=geometryInstances[index].planes[i];
        float padding=(dot(abs(plane.xyz),abs(center))+abs(plane.w))*8*selectionEpsilon;
        if(dot(plane.xyz,center)+plane.w+padding < -radius*length(plane.xyz))return false;
    }
    return true;
}
float selection_projected(float3 center,float radius,float error,float scale,float4 eyeFocal) {
    if(error==0)return 0;
    if(radius==selectionMaximum || !all(isfinite(center)))return selectionMaximum;
    float3 delta=center-eyeFocal.xyz;
    float padding=selection_length((abs(center)+abs(eyeFocal.xyz))*(4*selectionEpsilon));
    float distance=max(.001,(selection_length(delta)*(1-8*selectionEpsilon)-radius-padding)*(1-4*selectionEpsilon));
    float numerator=selection_upper(selection_upper(error*scale)*eyeFocal.w);
    if(numerator==selectionMaximum)return selectionMaximum;
    // Preserve positive geometric error even when FP32 division flushes subnormals.
    return max(1.175494351e-38,selection_upper(numerator/distance));
}
float instance_projected(uint index,float3 center,float radius,float error) {
    return selection_projected(center,radius,error,geometryInstances[index].errorScale.y,geometryInstances[index].eyeFocal);
}
bool instances_visible(float3 center,float radius) {
    for(uint i=0;i<geometryInstanceCount;++i) {
        float3 position;float extent;instance_sphere(i,center,radius,position,extent);
        if(instance_visible(i,position,extent))return true;
    }
    return false;
}
bool instances_refine(float3 center,float radius,float error,out float priority) {
    bool refine=false;priority=0;
    for(uint i=0;i<geometryInstanceCount;++i) {
        float3 position;float extent;instance_sphere(i,center,radius,position,extent);
        float pixels=instance_projected(i,position,extent,error);
        if(pixels>geometryInstances[i].errorScale.x && instance_visible(i,position,extent)) {
            refine=true;priority=max(priority,pixels);
        }
    }
    return refine;
}
float instances_error(float3 center,float radius,float error) {
    float maximum=0;
    for(uint i=0;i<geometryInstanceCount;++i) {
        float3 position;float extent;instance_sphere(i,center,radius,position,extent);
        maximum=max(maximum,instance_projected(i,position,extent,error));
    }
    return maximum;
}
