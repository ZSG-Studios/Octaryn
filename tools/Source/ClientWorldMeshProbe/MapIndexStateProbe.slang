#include "../../../octaryn-client/Shaders/Map/MapGeometry.slang"
#include "../../../octaryn-client/Shaders/Lighting/Surface.slang"
StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<MapRayMaterial> mapRayPrimitives;
StructuredBuffer<uint> mapIndices;
uniform float4 mapUniforms[11];
struct IndexSurface {float4 position:SV_Position;float3 normal:TEXCOORD0;};
IndexSurface index_vertex(uint id) {
    MapVertex v=mapVertices[id];
    float3 relative=map_vertex_position(v)-mapUniforms[2].xyz;
    float depth=dot(relative,mapUniforms[5].xyz);
    IndexSurface result;
    result.position=float4(dot(relative,mapUniforms[3].xyz)*mapUniforms[6].x,
        dot(relative,mapUniforms[4].xyz)*mapUniforms[6].y,
        depth*mapUniforms[6].z-mapUniforms[6].w,depth);
    result.normal=map_vertex_normal(v);return result;
}
[shader("vertex")]
IndexSurface vertex_main(uint id:SV_VertexID) {return index_vertex(id);}
[shader("vertex")]
IndexSurface forward_vertex_main(uint id:SV_VertexID) {return index_vertex(id);}
float4 index_color(bool front) {
    MapRayMaterial material=mapRayPrimitives[uint(mapUniforms[8].w)];
    if(!front && material.doubleSided==0)discard;
    // Force the SRV state used by forward RT, simultaneous with indexed fetch.
    return material.baseColor*(mapIndices[0]==0u?1.f:0.f);
}
struct IndexOutput {
    float4 color:SV_Target0;float4 relative:SV_Target1;float4 voxel:SV_Target2;
    float4 material:SV_Target3;float4 emission:SV_Target4;
};
[shader("fragment")]
IndexOutput fragment_main(IndexSurface input,bool front:SV_IsFrontFace) {
    IndexOutput result;
    result.color=index_color(front);result.relative=0;result.material=0;result.emission=0;
    uint packed=lighting_pack_map(front?input.normal:-input.normal,1);
    result.voxel=float4(packed&255u,(packed>>8)&255u,(packed>>16)&255u,packed>>24)/255.0;
    return result;
}
[shader("fragment")]
float4 forward_main(IndexSurface input,bool front:SV_IsFrontFace):SV_Target0 {return index_color(front);}
