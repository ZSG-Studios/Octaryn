// GLB map world raster: G-buffer entries matching WorldRaster.slang's terrain
// encoding, plus forward entries for the blended pass with scene fog.
#include "MapGeometry.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../Sky/Fog.slang"
StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<uint> mapIndices;
Texture2D<float4> baseColorTexture;
SamplerState mapSampler;
uniform float4 mapUniforms[9];
// 0 baseColor factor, 1 metallic/roughness/alphaCutoff/firstIndex,
// 2 cameraPosition, 3 cameraRight, 4 cameraUp, 5 cameraForward, 6 projection,
// 7 lightDirectionSky.xyz + sky visibility, 8 fogDistance/twilight/skylightFloor/0.
struct MapSurfaceOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float3 relative : TEXCOORD1;
    float2 uv : TEXCOORD2;
};
struct MapForwardOutput {
    float4 position : SV_Position;
    float3 relative : TEXCOORD0;
    float2 uv : TEXCOORD1;
};
// Lighting consumers decode one axis normal from the voxel MRT; pick the
// dominant interpolated-normal axis with its sign.
float3 map_axis_normal(float3 normal,out float index) {
    float3 absolute=abs(normal);
    if(absolute.y>=absolute.x && absolute.y>=absolute.z) {
        index=normal.y>0?4:5;
        return index==4?float3(0,1,0):float3(0,-1,0);
    }
    if(absolute.x>=absolute.z) {
        index=normal.x>0?2:3;
        return index==2?float3(1,0,0):float3(-1,0,0);
    }
    index=normal.z>0?0:1;
    return index==0?float3(0,0,1):float3(0,0,-1);
}
MapSurfaceOutput map_surface_vertex(uint id) {
    uint index=mapIndices[uint(mapUniforms[1].w)+id];
    MapVertex vertex=mapVertices[index];
    MapSurfaceOutput output;
    output.normal=map_vertex_normal(vertex);
    output.relative=map_vertex_position(vertex)-mapUniforms[2].xyz;
    // RHI uses a negative Vulkan viewport height; clip Y stays up.
    float depth=dot(output.relative,mapUniforms[5].xyz);
    output.position=float4(dot(output.relative,mapUniforms[3].xyz)*mapUniforms[6].x,
        dot(output.relative,mapUniforms[4].xyz)*mapUniforms[6].y,
        depth*mapUniforms[6].z-mapUniforms[6].w,depth);
    output.position.xy+=float2(mapUniforms[3].w,mapUniforms[4].w)*depth;
    output.uv=vertex.uv;
    return output;
}
[shader("vertex")]
MapSurfaceOutput vertex_main(uint id : SV_VertexID) {return map_surface_vertex(id);}
struct GBufferOutput {
    float4 color:SV_Target0;
    float4 relative:SV_Target1;
    float4 voxel:SV_Target2;
    float4 material:SV_Target3;
};
[shader("fragment")]
GBufferOutput fragment_main(MapSurfaceOutput input) {
    float4 sampled=baseColorTexture.SampleLevel(mapSampler,input.uv,0);
    float4 albedo=float4(sampled.rgb*mapUniforms[0].rgb,sampled.a*mapUniforms[0].a);
    if(mapUniforms[1].z>0 && albedo.a<mapUniforms[1].z) discard;
    float index;
    float3 axis=map_axis_normal(normalize(input.normal),index);
    GBufferOutput output;
    output.color=float4(albedo.rgb,
        original_ambient_visibility(axis,false,mapUniforms[8].z,mapUniforms[7].w));
    output.relative=float4(input.relative,dot(input.relative,mapUniforms[5].xyz));
    output.voxel=float4(index,0,0,0)/255.0;
    output.material=float4(saturate(mapUniforms[1].y),saturate(mapUniforms[1].x),.04,0);
    return output;
}
[shader("vertex")]
MapForwardOutput forward_vertex_main(uint id : SV_VertexID) {
    uint index=mapIndices[uint(mapUniforms[1].w)+id];
    MapForwardOutput output;
    output.relative=map_vertex_position(mapVertices[index])-mapUniforms[2].xyz;
    float depth=dot(output.relative,mapUniforms[5].xyz);
    output.position=float4(dot(output.relative,mapUniforms[3].xyz)*mapUniforms[6].x,
        dot(output.relative,mapUniforms[4].xyz)*mapUniforms[6].y,
        depth*mapUniforms[6].z-mapUniforms[6].w,depth);
    output.position.xy+=float2(mapUniforms[3].w,mapUniforms[4].w)*depth;
    output.uv=mapVertices[index].uv;
    return output;
}
[shader("fragment")]
float4 forward_main(MapForwardOutput input):SV_Target0 {
    float4 sampled=baseColorTexture.SampleLevel(mapSampler,input.uv,0);
    float4 albedo=float4(sampled.rgb*mapUniforms[0].rgb,sampled.a*mapUniforms[0].a);
    if(mapUniforms[1].z>0 && albedo.a<mapUniforms[1].z) discard;
    albedo.rgb=apply_scene_fog(albedo.rgb,input.relative,mapUniforms[7].w,
        mapUniforms[8].y,mapUniforms[8].x>0,mapUniforms[8].x);
    return albedo;
}
