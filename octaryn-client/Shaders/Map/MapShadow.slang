#include "MapGeometry.slang"

StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<MapRayMaterial> mapRayPrimitives;
uniform float4 shadowCenter;
uniform float4 shadowRight;
uniform float4 shadowUp;
uniform float4 shadowForward;
uniform uint shadowMaterial;

struct ShadowVertex {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    float2 uv1:TEXCOORD1;
    float4 color:TEXCOORD2;
};

[shader("vertex")]
ShadowVertex vertex_main(uint id:SV_VertexID) {
    MapVertex vertex=mapVertices[id];
    float3 relative=map_vertex_position(vertex)-shadowCenter.xyz;
    float depth=dot(relative,shadowForward.xyz);
    ShadowVertex output;
    output.position=float4(dot(relative,shadowRight.xyz)/shadowCenter.w,
        dot(relative,shadowUp.xyz)/shadowCenter.w,depth/2048+.5,1);
    output.uv=vertex.uv;output.uv1=vertex.uv1;output.color=vertex.color;
    return output;
}

[shader("fragment")]
void fragment_main(ShadowVertex input) {
    MapRayMaterial material=mapRayPrimitives[shadowMaterial];
    // Blended surfaces are transparent to the binary depth map. Cutouts use
    // the authored glTF threshold and sampler so raster and RT masks agree.
    if(material.alphaMode==2u)discard;
    float alpha=material.baseColor.a*input.color.a;
    if(material.textures[0].present!=0)
        alpha*=nonuniform(material.textures[0].image).SampleLevel(nonuniform(material.textures[0].sampler),
            map_texture_uv(material.textures[0],input.uv,input.uv1),0).a;
    if(material.alphaMode==1u && alpha<material.alphaCutoff)discard;
}
