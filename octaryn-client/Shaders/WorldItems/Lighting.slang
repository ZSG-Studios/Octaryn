#ifndef OCTARYN_ITEM_LIGHTING
#define OCTARYN_ITEM_LIGHTING
#include "../Materials/MetallicRoughness.slang"
float3 item_surface_lighting(float3 albedo,float metallic,float roughness,bool front,
    float3 normal,float3 view,float3 light,float strength,
    float3 incidentFront,float3 incidentBack,float4 directFront,float4 directBack) {
    float3 direction=normalize(normal)*(front?1:-1);
    float3 incident=front?incidentFront:incidentBack;
    float4 direct=front?directFront:directBack;
    float3 diffuse=albedo*(.96*(1-metallic))*(incident+direct.rgb);
    return diffuse+pbr_direct(albedo,metallic,roughness,direction,view,light)*strength*direct.w;
}
#endif
