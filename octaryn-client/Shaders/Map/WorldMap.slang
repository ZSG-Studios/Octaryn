#include "MapGeometry.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../Lighting/Surface.slang"
#include "../Materials/PbrEnvironment.slang"
#include "../Lighting/DiffuseEnvironment.slang"
#include "../Sky/Fog.slang"
#ifdef OCTARYN_MAP_FORWARD_RT
#include "../RayTracing/WorldRayQuery.slang"
#include "../Hdr/MapReflections.slang"
#endif
StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<MapRayMaterial> mapRayPrimitives;
uniform float4 mapUniforms[11];
struct MapSurfaceOutput {
    float4 position:SV_Position;
    float3 normal:TEXCOORD0;float3 relative:TEXCOORD1;
    float2 uv:TEXCOORD2;float2 uv1:TEXCOORD3;
    float4 tangent:TEXCOORD4;float4 color:TEXCOORD5;
    nointerpolation uint material:TEXCOORD6;
};
MapSurfaceOutput map_vertex(uint id) {
    MapVertex v=mapVertices[id];
    MapSurfaceOutput o;
    o.normal=map_vertex_normal(v);o.relative=map_vertex_position(v)-mapUniforms[2].xyz;
    float depth=dot(o.relative,mapUniforms[5].xyz);
    o.position=float4(dot(o.relative,mapUniforms[3].xyz)*mapUniforms[6].x,
        dot(o.relative,mapUniforms[4].xyz)*mapUniforms[6].y,
        depth*mapUniforms[6].z-mapUniforms[6].w,depth);
    o.position.xy+=float2(mapUniforms[3].w,mapUniforms[4].w)*depth;
    o.uv=v.uv;o.uv1=v.uv1;o.tangent=v.tangent;o.color=v.color;
    o.material=uint(mapUniforms[8].w);return o;
}
[shader("vertex")]
MapSurfaceOutput vertex_main(uint id:SV_VertexID) {return map_vertex(id);}
[shader("vertex")]
MapSurfaceOutput indirect_vertex_main(uint id:SV_VertexID,uint instance:SV_InstanceID,uint base:SV_StartInstanceLocation) {
    MapSurfaceOutput output=map_vertex(id);output.material=instance+base;return output;
}
[shader("vertex")]
MapSurfaceOutput forward_vertex_main(uint id:SV_VertexID) {return map_vertex(id);}
MapSurface map_fragment(MapSurfaceOutput input,bool front) {
    MapRayMaterial m=mapRayPrimitives[input.material];
    if(!front && m.doubleSided==0)discard;
    float3 normal=normalize(input.normal),tangent=input.tangent.xyz;
    float handedness=input.tangent.w;
    if(dot(tangent,tangent)<1e-10 || map_texture_frame_changed(m.textures[2])) {
        float2 uv=map_texture_uv(m.textures[2],input.uv,input.uv1);
        float2 a=ddx(uv),b=ddy(uv);
        float3 p=ddx(input.relative),q=ddy(input.relative);
        float determinant=a.x*b.y-a.y*b.x;
        tangent=abs(determinant)>1e-10?(p*b.y-q*a.y)/determinant:float3(0);
        float3 bitangent=abs(determinant)>1e-10?(q*a.x-p*b.x)/determinant:float3(0);
        handedness=dot(cross(normal,tangent),bitangent)<0?-1:1;
    }
    MapSurface s=map_surface(m,input.uv,input.uv1,input.color,normal,tangent,handedness,
        ddx(input.uv),ddy(input.uv),ddx(input.uv1),ddy(input.uv1));
    if(!front)s.normal=-s.normal;
    if(m.alphaMode==1 && s.albedo.a<m.alphaCutoff)discard;
    return s;
}
struct GBufferOutput {
    float4 color:SV_Target0;float4 relative:SV_Target1;float4 voxel:SV_Target2;
    float4 material:SV_Target3;float4 emission:SV_Target4;
    uint surfaceFlags:SV_Target5;
};
GBufferOutput map_gbuffer(MapSurfaceOutput input,bool front) {
    MapSurface s=map_fragment(input,front);GBufferOutput o;
    o.color=float4(s.albedo.rgb,original_ambient_visibility(s.normal,false,mapUniforms[8].z,mapUniforms[7].w));
    o.relative=float4(input.relative,dot(input.relative,mapUniforms[5].xyz));
    uint packed=lighting_pack_map(s.normal,s.occlusion);
    o.voxel=float4(packed&255u,(packed>>8)&255u,(packed>>16)&255u,packed>>24)/255.0;
    o.material=float4(s.roughness,s.metallic,.04,0);o.emission=float4(s.emission,0);
    o.surfaceFlags=mapRayPrimitives[input.material].alphaMode==0?1u:0u;return o;
}
[shader("fragment")]
GBufferOutput fragment_main(MapSurfaceOutput input,bool front:SV_IsFrontFace) {return map_gbuffer(input,front);}
[shader("fragment")]
[earlydepthstencil]
float4 forward_main(MapSurfaceOutput input,bool front:SV_IsFrontFace):SV_Target0 {
    MapSurface s=map_fragment(input,front);
    if(s.albedo.a<=0)discard;
    float4 sun=mapUniforms[9],sky=mapUniforms[10];
    float3 view=-input.relative/max(length(input.relative),1e-5);
    float visibility=1;
#ifdef OCTARYN_MAP_FORWARD_RT
    float3 position=input.relative+mapUniforms[2].xyz;
    visibility=dot(s.normal,sun.xyz)>0?world_ray_visibility(position,s.normal,sun.xyz):0;
#endif
    float3 hdr=s.emission+
        pbr_direct(s.albedo.rgb,s.metallic,s.roughness,s.normal,view,sun.xyz)*sun.w*visibility;
    float ambient_visibility=original_ambient_visibility(s.normal,false,mapUniforms[8].z,mapUniforms[7].w);
    hdr+=s.albedo.rgb*(1-s.metallic)*
        diffuse_environment(s.normal,sun.xyz,sky)*ambient_visibility*s.occlusion;
#ifdef OCTARYN_MAP_FORWARD_RT
    hdr+=map_reflections(position,s.normal,view,s.albedo.rgb,s.metallic,s.roughness,sun,sky);
#endif
    return float4(apply_scene_fog(hdr,input.relative,sky.x,sky.z,sky.w>0,sky.w),s.albedo.a);
}
