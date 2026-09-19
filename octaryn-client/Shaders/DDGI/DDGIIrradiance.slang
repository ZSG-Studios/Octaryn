#ifndef OCTARYN_DDGI_IRRADIANCE
#define OCTARYN_DDGI_IRRADIANCE
#include "DDGITypes.slang"
bool ddgi_geometry_locked(DDGIControl control,DDGIProbe previous,bool reset) {
    // Native LightingOnly (bit 2) is cleared by any concurrent geometry edit.
    bool changed=control.refreshFrame>previous.metadata.y;
    bool geometry=changed&&(control.padding.z&4u)==0;
    return !reset&&!geometry;
}
float3 ddgi_integrate_irradiance(StructuredBuffer<DDGIRay> rays,uint base,uint first,uint count,float3 direction) {
    float3 sum=0;float weightSum=0;
    for(uint i=first;i<count;++i) {
        DDGIRay ray=rays[base+i];
        float weight=max(dot(direction,ray.directionHit.xyz),0);
        // Backfaces contribute zero energy, not a smaller hemisphere.
        if(ray.radianceDistance.w>=0)sum+=ray.radianceDistance.rgb*weight;
        weightSum+=weight;
    }
    // Store physical irradiance: a constant environment integrates to pi*L.
    return 3.14159265359*sum/max(weightSum,1e-5);
}
float ddgi_history_retention(float hysteresis,uint samples,uint reactive,bool reset) {
    // A new cell or relocated probe owns no valid observation yet.
    if(reset)return 0;
    // Idle frames contain no new ray observations and must not erase history.
    float warmup=float(samples)/float(samples+1);
    float mature=saturate(hysteresis);
    // An explicit lighting or geometry change adopts the bounded reactive policy
    // so one noisy ray sample never replaces the accumulated field outright.
    float lean=reactive>0?min(.5,mature):mature;
    return min(min(mature,warmup),lean);
}
float3 ddgi_blend_irradiance(float3 incoming,float3 previous,float history) {
    // A one-sided brightness clamp biases stationary sparse lighting dark.
    // Keep the linear estimator; explicit source edits already reject history.
    return lerp(incoming,previous,history);
}
#endif
