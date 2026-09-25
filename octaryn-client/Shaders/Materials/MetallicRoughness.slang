#ifndef OCTARYN_METALLIC_ROUGHNESS
#define OCTARYN_METALLIC_ROUGHNESS
// glTF metallic-roughness BRDF: GGX distribution, Smith visibility, Schlick Fresnel.
float3 pbr_fresnel(float3 f0,float cosine) {
    return f0+(1-f0)*pow(1-saturate(cosine),5);
}
float pbr_smith_g1(float cosine,float alphaSquared) {
    return 2*cosine/max(cosine+sqrt(alphaSquared+(1-alphaSquared)*cosine*cosine),1e-6);
}
float3 pbr_direct(float3 albedo,float metallic,float roughness,float3 normal,float3 view,float3 light) {
    float nl=saturate(dot(normal,light)),nv=saturate(dot(normal,view));
    if(nl<=0 || nv<=0)return 0;
    float3 halfway=view+light;
    halfway/=max(length(halfway),1e-6);
    float nh=saturate(dot(normal,halfway)),vh=saturate(dot(view,halfway));
    float alpha=max(roughness*roughness,.0025),a2=alpha*alpha;
    float denominator=nh*nh*(a2-1)+1;
    float distribution=a2/max(3.14159265359*denominator*denominator,1e-12);
    float3 fresnel=pbr_fresnel(lerp(float3(.04),albedo,metallic),vh);
    float geometry=pbr_smith_g1(nl,a2)*pbr_smith_g1(nv,a2);
    return (1-fresnel)*albedo*(1-metallic)*nl/3.14159265359+
        fresnel*(distribution*geometry/max(4*nv,1e-6));
}
#endif
