#ifndef OCTARYN_PBR_ENVIRONMENT
#define OCTARYN_PBR_ENVIRONMENT
#include "MetallicRoughness.slang"
// Karis/Lazarov analytic environment BRDF, not the direct-light Fresnel term.
// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
float3 pbr_environment_reflectance(float3 f0,float roughness,float nv) {
    float4 r=saturate(roughness)*float4(-1,-.0275,-.572,.022)+float4(1,.0425,1.04,-.04);
    float a=min(r.x*r.x,exp2(-9.28*saturate(nv)))*r.x+r.y;
    float2 ab=float2(-1.04,1.04)*a+r.zw;
    return saturate(f0*ab.x+ab.y);
}
// Heitz 2018 GGX visible-normal sampling in a local frame with normal +Z.
// https://jcgt.org/published/0007/04/01/paper.pdf
float3 pbr_visible_normal(float3 view,float alpha,float2 sample) {
    float3 stretched=normalize(float3(alpha*view.xy,view.z));
    float lengthSquared=dot(stretched.xy,stretched.xy);
    float3 tangent=lengthSquared>0?float3(-stretched.y,stretched.x,0)/sqrt(lengthSquared):float3(1,0,0);
    float3 bitangent=cross(stretched,tangent);
    float radius=sqrt(sample.x),phi=6.28318530718*sample.y;
    float x=radius*cos(phi),y=radius*sin(phi);
    float blend=.5*(1+stretched.z);
    y=(1-blend)*sqrt(max(0,1-x*x))+blend*y;
    float3 disk=x*tangent+y*bitangent+sqrt(max(0,1-x*x-y*y))*stretched;
    return normalize(float3(alpha*disk.xy,max(0,disk.z)));
}
#endif
