struct PlayerVertex {
    float4 position,normal,uv;
    uint4 joints;
    float4 weights;
};
StructuredBuffer<PlayerVertex> vertices;
StructuredBuffer<uint> indices;
uniform float4 jointColumns[256];
uniform uint vertexCount;
uniform float4 feetPosition,bodyRotation;
RWStructuredBuffer<float4> skinnedOutput;
[shader("compute")]
[numthreads(64,1,1)]
void skin_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=vertexCount)return;
    PlayerVertex vertex=vertices[id.x];float4 skinned=0;
    for(uint k=0;k<4;++k) {
        uint joint=vertex.joints[k]*4;
        skinned+=(jointColumns[joint]*vertex.position.x+jointColumns[joint+1]*vertex.position.y+
            jointColumns[joint+2]*vertex.position.z+jointColumns[joint+3])*vertex.weights[k];
    }
    skinnedOutput[id.x]=float4(feetPosition.xyz+float3(
        bodyRotation.x*skinned.x-bodyRotation.y*skinned.z,skinned.y,
        bodyRotation.y*skinned.x+bodyRotation.x*skinned.z),1);
}
StructuredBuffer<float4> skinnedPositions;
uniform float4 shadowCenter,shadowRight,shadowUp,shadowForward,shadowProjection;
uniform uint firstIndex;
struct ShadowVertex {float4 position:SV_Position;};
[shader("vertex")]
ShadowVertex vertex_main(uint id:SV_VertexID) {
    float3 p=skinnedPositions[indices[firstIndex+id]].xyz-shadowCenter.xyz;
    float z=dot(p,shadowForward.xyz);ShadowVertex result;
    if(shadowProjection.w>0)
        result.position=float4(dot(p,shadowRight.xyz),dot(p,shadowUp.xyz),
            z*shadowProjection.x-shadowProjection.y,z);
    else result.position=float4(dot(p,shadowRight.xyz)/shadowCenter.w,
        dot(p,shadowUp.xyz)/shadowCenter.w,z/2048+.5,1);
    return result;
}
[shader("fragment")]
void fragment_main(ShadowVertex input) {}
