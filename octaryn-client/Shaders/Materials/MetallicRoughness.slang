#ifndef OCTARYN_METALLIC_ROUGHNESS
#define OCTARYN_METALLIC_ROUGHNESS
#include "ZeroTangentPlane.slang"
// glTF metallic-roughness BRDF: GGX distribution, Smith visibility, Schlick Fresnel.
float3 pbr_fresnel(float3 f0,float cosine) {
    return f0+(1-f0)*pow(1-saturate(cosine),5);
}
float pbr_smith_g1(float cosine,float alphaSquared) {
    return 2*cosine/max(cosine+sqrt(alphaSquared+(1-alphaSquared)*cosine*cosine),1e-6);
}
float3 pbr_direct_angles(float3 albedo,float metallic,float roughness,float4 angles) {
    float nl=angles.x,nv=angles.y,nh=angles.z,vh=angles.w;
    if(nl<=0 || nv<=0)return 0;
    float alpha=max(roughness*roughness,.0025),a2=alpha*alpha;
    float denominator=nh*nh*(a2-1)+1;
    float distribution=a2/max(3.14159265359*denominator*denominator,1e-12);
    float3 fresnel=pbr_fresnel(lerp(float3(.04),albedo,metallic),vh);
    float geometry=pbr_smith_g1(nl,a2)*pbr_smith_g1(nv,a2);
    return (1-fresnel)*albedo*(1-metallic)*nl/3.14159265359+
        fresnel*(distribution*geometry/max(4*nv,1e-6));
}
float3 pbr_direct(float3 albedo,float metallic,float roughness,float3 normal,float3 view,float3 light) {
    float3 h=view+light;h/=max(length(h),1e-6);
    return pbr_direct_angles(albedo,metallic,roughness,saturate(float4(dot(normal,light),dot(normal,view),dot(normal,h),dot(view,h))));
}
float3 pbr_direct_zero_plane(float3 albedo,float metallic,float roughness,float3 normal,float3 view,float3 light,float sampleZ) {
    return pbr_direct_angles(albedo,metallic,roughness,zero_plane_angles(normal,view,light,sampleZ));
}
#endif
