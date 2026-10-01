#ifndef OCTARYN_MAP_RAY_GEOMETRY
#define OCTARYN_MAP_RAY_GEOMETRY
#include "../Map/MapGeometry.slang"
struct MapRayGeometry {
    ByteAddressBuffer.Handle vertices;
    ByteAddressBuffer.Handle indices;
    ByteAddressBuffer.Handle materials;
    ByteAddressBuffer.Handle triangles;
    uint firstTriangle;
    uint clustered;
    uint2 transformPadding;
    float4 world[3];
    float4 normal[3];
    float orientation;
    uint vertexStride;
    uint2 tailPadding;
};
StructuredBuffer<MapRayGeometry> mapRayMeshes;
bool map_ray_dynamic_instance(uint instance,uint firstDynamic) {
    return is_map_ray_instance(instance) && (instance&0x7fffffu)>=firstDynamic;
}
MapVertex map_ray_vertex(uint instance,uint index) {
    MapRayGeometry mesh=mapRayMeshes[instance&0x7fffffu];
    uint vertex=nonuniform(mesh.indices).Load<uint>(index*4);
    MapVertex value;
    if(mesh.vertexStride==12) {
        float3 position=asfloat(nonuniform(mesh.vertices).Load3(vertex*12));
        for(uint row=0;row<3;++row) {value.position[row]=position[row];value.normal[row]=0;}
        value.uv=0;value.uv1=0;value.padding=0;value.tangent=0;value.color=1;
    } else if((mesh.clustered&1u)!=0) {
        uint offset=vertex*72;
        for(uint row=0;row<3;++row) {
            value.position[row]=asfloat(nonuniform(mesh.vertices).Load(offset+row*4));
            value.normal[row]=asfloat(nonuniform(mesh.vertices).Load(offset+12+row*4));
        }
        value.uv=asfloat(nonuniform(mesh.vertices).Load2(offset+24));
        value.uv1=asfloat(nonuniform(mesh.vertices).Load2(offset+32));value.padding=0;
        value.tangent=asfloat(nonuniform(mesh.vertices).Load4(offset+40));
        value.color=asfloat(nonuniform(mesh.vertices).Load4(offset+56));
    } else value=nonuniform(mesh.vertices).Load<MapVertex>(vertex*80);
    // Keep the raw-buffer aggregate in scope; Slang 2026.17.1 crashes compiling
    // its transformed helper return to SPIR-V. Math matches MapTransform.slang.
    if((mesh.clustered&2u)!=0) {
        float4 position=float4(map_vertex_position(value),1);
        float3 normal=map_vertex_normal(value),tangent=value.tangent.xyz;
        for(uint row=0;row<3;++row) {
            value.position[row]=dot(mesh.world[row],position);
            value.normal[row]=dot(mesh.normal[row].xyz,normal);
            value.tangent[row]=dot(mesh.world[row].xyz,tangent);
        }
        normal=map_vertex_normal(value);
        if(dot(normal,normal)>0)normal=normalize(normal);
        for(uint row=0;row<3;++row)value.normal[row]=normal[row];
        tangent=value.tangent.xyz-normal*dot(normal,value.tangent.xyz);
        if(dot(tangent,tangent)>0)tangent=normalize(tangent);
        value.tangent.xyz=tangent;value.tangent.w*=mesh.orientation;
    }
    return value;
}
float map_ray_orientation(uint instance) {return mapRayMeshes[instance&0x7fffffu].orientation;}
MapRayMaterial map_ray_material(uint instance,uint index) {
    return nonuniform(mapRayMeshes[instance&0x7fffffu].materials).Load<MapRayMaterial>(index*304);
}
// Cluster batches share one BLAS geometry; authored material identity is per triangle.
uint2 map_ray_identity(uint instance,uint geometry,uint primitive) {
    MapRayGeometry mesh=mapRayMeshes[instance&0x7fffffu];
    if(mesh.clustered!=0) {
        uint triangle=mesh.firstTriangle+primitive;
        uint4 record=nonuniform(mesh.triangles).Load<uint4>(triangle*16);
        return uint2(record.z,triangle);
    }
    return uint2(geometry,map_ray_material(instance,geometry).padding.x+primitive);
}
#endif
