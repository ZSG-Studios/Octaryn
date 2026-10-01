#ifndef OCTARYN_BLOCK_TRANSPORT_REFLECTION
#define OCTARYN_BLOCK_TRANSPORT_REFLECTION
#include "Sample.slang"
#include "../World/WorldPlantSurface.slang"
#include "../Materials/MetallicRoughness.slang"
StructuredBuffer<float4> btLookupDirect;
bool sample_block_transport_reflected_incident(uint4 packed,bool backFace,out float3 value) {
    value=0;
    // Cube cutout back faces do not own the stored outward hemisphere.
    if(backFace && (packed.w==0 || !world_plant_tag(packed.w-1)))return false;
    uint row;if(!bt_lookup_surface(packed,row))return false;
    bool complete;value=bt_lookup_radiance(row,complete);
    float4 direct=btLookupDirect[row];
    if(!complete || !bt_epoch(direct.w,btLookupInfo.z))return false;
    value+=direct.rgb;return true;
}
float3 bt_reflected_specular_sun(float3 albedo,float metallic,float roughness,
    float3 normal,float3 view,float3 light) {
    float nl=saturate(dot(normal,light)),nv=saturate(dot(normal,view));
    if(nl<=0 || nv<=0)return 0;
    float3 halfway=view+light;halfway/=max(length(halfway),1e-6);
    float nh=saturate(dot(normal,halfway)),vh=saturate(dot(view,halfway));
    float alpha=max(roughness*roughness,.0025),a2=alpha*alpha;
    float denominator=nh*nh*(a2-1)+1;
    float distribution=a2/max(3.14159265359*denominator*denominator,1e-12);
    float3 fresnel=pbr_fresnel(lerp(float3(.04),albedo,metallic),vh);
    float geometry=pbr_smith_g1(nl,a2)*pbr_smith_g1(nv,a2);
    return fresnel*(distribution*geometry/max(4*nv,1e-6));
}
float3 bt_reflected_sun_brdf(bool cachedDiffuse,float3 albedo,float metallic,float roughness,
    float3 normal,float3 view,float3 light) {
    if(cachedDiffuse)return bt_reflected_specular_sun(albedo,metallic,roughness,normal,view,light);
    return pbr_direct(albedo,metallic,roughness,normal,view,light);
}
#endif
