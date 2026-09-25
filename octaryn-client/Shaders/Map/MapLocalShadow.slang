#include "MapGeometry.slang"

StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<MapRayMaterial> mapRayPrimitives;
uniform float4 localShadowPosition;
uniform float4 localShadowProjection;
uniform float4 localRight;
uniform float4 localUp;
uniform float4 localForward;
uniform uint localShadowMaterial;

struct ShadowVertex {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    float2 uv1:TEXCOORD1;
    float4 color:TEXCOORD2;
};

[shader("vertex")]
ShadowVertex vertex_main(uint id:SV_VertexID) {
    MapVertex vertex=mapVertices[id];
    float3 relative=map_vertex_position(vertex)-localShadowPosition.xyz;
    float depth=dot(relative,localForward.xyz);
    ShadowVertex output;
    output.position=float4(dot(relative,localRight.xyz),dot(relative,localUp.xyz),
        depth*localShadowProjection.x-localShadowProjection.y,depth);
    output.uv=vertex.uv;output.uv1=vertex.uv1;output.color=vertex.color;
    return output;
}

[shader("fragment")]
void fragment_main(ShadowVertex input) {
    MapRayMaterial material=mapRayPrimitives[localShadowMaterial];
    if(material.alphaMode==2u)discard;
    float alpha=material.baseColor.a*input.color.a;
    if(material.textures[0].present!=0)
        alpha*=nonuniform(material.textures[0].image).SampleLevel(nonuniform(material.textures[0].sampler),
            map_texture_uv(material.textures[0],input.uv,input.uv1),0).a;
    if(material.alphaMode==1u && alpha<material.alphaCutoff)discard;
}
