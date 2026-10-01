#ifndef OCTARYN_MAP_TRANSFORM_SLANG
#define OCTARYN_MAP_TRANSFORM_SLANG
#include "MapGeometry.slang"
MapVertex map_transform_vertex(MapVertex vertex,float4 world0,float4 world1,float4 world2,
    float4 normal0,float4 normal1,float4 normal2,float orientation) {
    float4 point=float4(map_vertex_position(vertex),1);
    float3 normal=map_vertex_normal(vertex),tangent=vertex.tangent.xyz;
    float3 worldPosition=float3(dot(world0,point),dot(world1,point),dot(world2,point));
    float3 worldNormal=float3(dot(normal0.xyz,normal),dot(normal1.xyz,normal),dot(normal2.xyz,normal));
    float3 worldTangent=float3(dot(world0.xyz,tangent),dot(world1.xyz,tangent),dot(world2.xyz,tangent));
    if(dot(worldNormal,worldNormal)>0)worldNormal=normalize(worldNormal);
    worldTangent-=worldNormal*dot(worldNormal,worldTangent);
    if(dot(worldTangent,worldTangent)>0)worldTangent=normalize(worldTangent);
    for(uint axis=0;axis<3;++axis) {vertex.position[axis]=worldPosition[axis];vertex.normal[axis]=worldNormal[axis];}
    vertex.tangent.xyz=worldTangent;vertex.tangent.w*=orientation;return vertex;
}
#endif
