#include "WorldMap.slang"
struct MapMeshlet {
    uint vertexOffset,triangleOffset,vertexCount,triangleCount;
    uint material,padding0,padding1,padding2;
    float4 sphere;
    float4 coneApexCutoff;
    float4 coneAxis;
};
StructuredBuffer<MapMeshlet> mapMeshlets;
StructuredBuffer<uint> mapMeshletVertices;
StructuredBuffer<uint> mapMeshletTriangles;
uniform float4 mapMeshletPlanes[6];
uniform uint4 mapMeshletSettings;
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128,1,1)]
void mesh_main(uint3 group:SV_GroupID,uint thread:SV_GroupIndex,
    out vertices MapSurfaceOutput vertices[128],out indices uint3 triangles[256]) {
    uint index=group.x+group.y*mapMeshletSettings.y;
    bool visible=index<mapMeshletSettings.x;
    MapMeshlet mesh=(MapMeshlet)0;
    if(visible) {
        mesh=mapMeshlets[index];
        if(mapMeshletSettings.z!=0) {
            float3 relative=mesh.sphere.xyz-mapUniforms[2].xyz;
            [unroll]for(uint plane=0;plane<6;++plane) {
                float4 p=mapMeshletPlanes[plane];
                visible=visible && dot(p.xyz,relative)+p.w>=-mesh.sphere.w;
            }
        }
        // Backface cluster culling (meshoptimizer normal cone). Degenerate cones
        // (double-sided or scattered normals) have axis 0/cutoff 1 and never cull.
        // NaN comparisons are false, so a camera at the apex keeps the meshlet.
        if(visible && mapMeshletSettings.w!=0) {
            float3 view=normalize(mesh.coneApexCutoff.xyz-mapUniforms[2].xyz);
            visible=!((dot(view,mesh.coneAxis.xyz)>=mesh.coneApexCutoff.w));
        }
    }
    uint vertexCount=mesh.vertexCount*uint(visible),triangleCount=mesh.triangleCount*uint(visible);
    SetMeshOutputCounts(vertexCount,triangleCount);
    if(thread<vertexCount) {
        MapSurfaceOutput vertex=map_vertex(mapMeshletVertices[mesh.vertexOffset+thread]);
        vertex.material=mesh.material;vertices[thread]=vertex;
    }
    for(uint triangle=thread;triangle<triangleCount;triangle+=128) {
        uint packed=mapMeshletTriangles[mesh.triangleOffset+triangle];
        triangles[triangle]=uint3(packed&255,(packed>>8)&255,(packed>>16)&255);
    }
}
