#include "WorldMap.slang"
struct MapMeshlet {
    uint vertexOffset,triangleOffset,vertexCount,triangleCount;
    uint material,padding0,padding1,padding2;
    float4 sphere;
    float4 coneApexCutoff;
    float4 coneAxis;
};
// Amplification compaction (Wihlidal, "GPU-Driven Rendering Pipelines"; the
// DirectX meshlet amplification sample): one 32-thread task group tests 32
// meshlets against the frustum and backface cones, then dispatches mesh shader
// groups only for survivors, so culled clusters never launch a mesh group.
static const uint kAmplificationSize=32;
struct MapMeshletPayload { uint indices[kAmplificationSize]; };
StructuredBuffer<MapMeshlet> mapMeshlets;
StructuredBuffer<uint> mapMeshletVertices;
StructuredBuffer<uint> mapMeshletTriangles;
uniform float4 mapMeshletPlanes[6];
uniform uint4 mapMeshletSettings;
groupshared uint amplificationCount;
groupshared MapMeshletPayload amplificationPayload;
bool meshlet_visible(MapMeshlet mesh) {
    if(mapMeshletSettings.z!=0) {
        float3 relative=mesh.sphere.xyz-mapUniforms[2].xyz;
        [unroll]for(uint plane=0;plane<6;++plane) {
            float4 p=mapMeshletPlanes[plane];
            if(dot(p.xyz,relative)+p.w<-mesh.sphere.w)return false;
        }
    }
    // Backface cluster culling (meshoptimizer normal cone). Degenerate cones
    // (double-sided or scattered normals) have axis 0/cutoff 1 and never cull.
    // NaN comparisons are false, so a camera at the apex keeps the meshlet.
    if(mapMeshletSettings.w!=0) {
        float3 view=normalize(mesh.coneApexCutoff.xyz-mapUniforms[2].xyz);
        if(dot(view,mesh.coneAxis.xyz)>=mesh.coneApexCutoff.w)return false;
    }
    return true;
}
[shader("amplification")]
[numthreads(kAmplificationSize,1,1)]
void amplification_main(uint3 group:SV_GroupID,uint thread:SV_GroupIndex) {
    uint index=group.x*kAmplificationSize+thread;
    bool visible=index<mapMeshletSettings.x && meshlet_visible(mapMeshlets[index]);
    if(thread==0)amplificationCount=0;
    GroupMemoryBarrierWithGroupSync();
    if(visible) {
        uint slot;
        InterlockedAdd(amplificationCount,1,slot);
        amplificationPayload.indices[slot]=index;
    }
    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(amplificationCount,1,1,amplificationPayload);
}
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128,1,1)]
void mesh_main(uint3 group:SV_GroupID,uint thread:SV_GroupIndex,
    in payload MapMeshletPayload payload,
    out vertices MapSurfaceOutput vertices[128],out indices uint3 triangles[256]) {
    MapMeshlet mesh=mapMeshlets[payload.indices[group.x]];
    SetMeshOutputCounts(mesh.vertexCount,mesh.triangleCount);
    if(thread<mesh.vertexCount) {
        MapSurfaceOutput vertex=map_vertex(mapMeshletVertices[mesh.vertexOffset+thread]);
        vertex.material=mesh.material;vertices[thread]=vertex;
    }
    for(uint triangle=thread;triangle<mesh.triangleCount;triangle+=128) {
        uint packed=mapMeshletTriangles[mesh.triangleOffset+triangle];
        triangles[triangle]=uint3(packed&255,(packed>>8)&255,(packed>>16)&255);
    }
}
