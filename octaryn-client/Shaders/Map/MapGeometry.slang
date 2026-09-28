#ifndef OCTARYN_MAP_GEOMETRY_SLANG
#define OCTARYN_MAP_GEOMETRY_SLANG
static const uint kMapRayInstanceId=0x800000u;
bool is_map_ray_instance(uint instance) {return (instance&kMapRayInstanceId)!=0;}
struct MapVertex {
    float position[3];float normal[3];float2 uv;
    float2 uv1;float2 padding;float4 tangent;float4 color;
};
struct MapTexture {
    Texture2D<float4>.Handle image;
    SamplerState.Handle sampler;
    float transform[6];uint texcoord;uint present;
};
struct MapRayMaterial {
    float4 baseColor;float3 emissive;float normalScale;
    float metallic;float roughness;float alphaCutoff;float occlusionStrength;
    uint alphaMode;uint doubleSided;uint2 padding;
    MapTexture textures[5];
};
float3 map_vertex_position(MapVertex v) {return float3(v.position[0],v.position[1],v.position[2]);}
float3 map_vertex_normal(MapVertex v) {return float3(v.normal[0],v.normal[1],v.normal[2]);}
float2 map_texture_uv(MapTexture t,float2 uv,float2 uv1) {
    float2 p=t.texcoord==1?uv1:uv;
    return float2(t.transform[0]*p.x+t.transform[1]*p.y+t.transform[2],
        t.transform[3]*p.x+t.transform[4]*p.y+t.transform[5]);
}
bool map_texture_frame_changed(MapTexture t) {
    return t.texcoord!=0 || t.transform[0]!=1 || t.transform[1]!=0 ||
        t.transform[3]!=0 || t.transform[4]!=1;
}
float4 map_texture_sample(MapTexture t,float2 uv,float2 uv1,float2 dx,float2 dy,float2 dx1,float2 dy1) {
    if(t.present==0)return float4(1);
    float2 gx=t.texcoord==1?dx1:dx,gy=t.texcoord==1?dy1:dy;
    float2 tx=float2(t.transform[0]*gx.x+t.transform[1]*gx.y,t.transform[3]*gx.x+t.transform[4]*gx.y);
    float2 ty=float2(t.transform[0]*gy.x+t.transform[1]*gy.y,t.transform[3]*gy.x+t.transform[4]*gy.y);
    return nonuniform(t.image).SampleGrad(nonuniform(t.sampler),map_texture_uv(t,uv,uv1),tx,ty);
}
struct MapSurface {float4 albedo;float3 normal;float3 emission;float metallic;float roughness;float occlusion;};
MapSurface map_surface(MapRayMaterial m,float2 uv,float2 uv1,float4 color,float3 normal,
    float3 tangent,float handedness,float2 dx,float2 dy,float2 dx1,float2 dy1) {
    MapSurface s;
    s.albedo=m.baseColor*color*map_texture_sample(m.textures[0],uv,uv1,dx,dy,dx1,dy1);
    if(m.alphaMode==0)s.albedo.a=1;
    float4 mr=map_texture_sample(m.textures[1],uv,uv1,dx,dy,dx1,dy1);
    s.metallic=saturate(m.metallic*mr.b);s.roughness=saturate(m.roughness*mr.g);
    s.emission=m.emissive*map_texture_sample(m.textures[4],uv,uv1,dx,dy,dx1,dy1).rgb;
    s.occlusion=lerp(1,map_texture_sample(m.textures[3],uv,uv1,dx,dy,dx1,dy1).r,m.occlusionStrength);
    s.normal=normalize(normal);
    tangent-=s.normal*dot(s.normal,tangent);
    if(m.textures[2].present!=0 && dot(tangent,tangent)>1e-10) {
        tangent=normalize(tangent);
        float3 n=map_texture_sample(m.textures[2],uv,uv1,dx,dy,dx1,dy1).rgb*2-1;
        n.xy*=m.normalScale;
        s.normal=normalize(tangent*n.x+cross(s.normal,tangent)*handedness*n.y+s.normal*n.z);
    }
    return s;
}
#endif
