#include "../Map/MapGeometry.slang"
StructuredBuffer<MapVertex> coverageVertices;
struct CoverageInstance {float4 current;float4 previous;};
StructuredBuffer<CoverageInstance> coverageInstances;
uniform float4 coverageView[5];
float4 coverage_project(float3 position) {
    float3 relative=position-coverageView[0].xyz;
    float depth=dot(relative,coverageView[3].xyz);
    float4 result=float4(dot(relative,coverageView[1].xyz)*coverageView[4].x,
        dot(relative,coverageView[2].xyz)*coverageView[4].y,
        depth*coverageView[4].z-coverageView[4].w,depth);
    result.xy+=float2(coverageView[1].w,coverageView[2].w)*depth;return result;
}
[shader("vertex")]
float4 coverage_vertex(uint id:SV_VertexID):SV_Position {
    return coverage_project(map_vertex_position(coverageVertices[id]));
}
[shader("vertex")]
float4 coverage_item_vertex(uint id:SV_VertexID,uint instance:SV_InstanceID,uint base:SV_StartInstanceLocation):SV_Position {
    return coverage_project(map_vertex_position(coverageVertices[id])+coverageInstances[instance+base].current.xyz);
}
[shader("fragment")]
uint coverage_fragment():SV_Target0 {return 2u;}
