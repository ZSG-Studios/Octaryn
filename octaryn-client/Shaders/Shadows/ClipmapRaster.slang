#include "../Materials/MaterialSampling.slang"
#include "../Voxel/WorldGeometry.slang"
StructuredBuffer<uint4> faces;
StructuredBuffer<FluidFace> fluidFaces;
uniform uint fluidBase;
uniform float4 center;
uniform float4 lightRight;
uniform float4 lightUp;
uniform float4 lightForward;
struct Vertex {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    nointerpolation uint voxel:TEXCOORD1;
    nointerpolation uint flags:TEXCOORD2;
};
Vertex shadow_vertex(uint vertex,uint4 face,FluidFace fluid) {
    WorldGeometryVertex geometry=world_geometry_corner(vertex,face,fluid);
    float3 p=float3(asint(face.xyz))+geometry.offset-center.xyz;
    Vertex result;result.position=float4(dot(p,lightRight.xyz)/center.w,dot(p,lightUp.xyz)/center.w,
        dot(p,lightForward.xyz)/2048+.5,1);
    result.uv=geometry.uv;result.voxel=geometry.voxel;result.flags=geometry.flags;return result;
}
[shader("vertex")]
Vertex vertex_main(uint vertex:SV_VertexID,uint localInstance:SV_InstanceID,uint baseInstance:SV_StartInstanceLocation) {
    uint instance=localInstance+baseInstance;
    uint4 face=faces[instance];FluidFace fluid;fluid.heights=0;fluid.flow=0;
    if((blockMaterials[face.w&65535].flags&4)!=0)fluid=fluidFaces[instance-fluidBase];
    return shadow_vertex(vertex,face,fluid);
}
struct ShadowBatchRecord {
    ByteAddressBuffer.Handle faceBuffer;
    ByteAddressBuffer.Handle fluidBuffer;
    uint firstFace;
    uint fluidBase;
};
StructuredBuffer<ShadowBatchRecord> shadowRecords;
[shader("vertex")]
Vertex batch_vertex_main(uint vertex:SV_VertexID,uint localInstance:SV_InstanceID,uint recordIndex:SV_StartInstanceLocation) {
    ShadowBatchRecord record=shadowRecords[recordIndex];
    uint instance=record.firstFace+localInstance;
    uint4 face=nonuniform(record.faceBuffer).Load<uint4>(instance*16);
    FluidFace fluid;fluid.heights=0;fluid.flow=0;
    if((blockMaterials[face.w&65535].flags&4)!=0)fluid=nonuniform(record.fluidBuffer).Load<FluidFace>((instance-record.fluidBase)*32);
    return shadow_vertex(vertex,face,fluid);
}
[shader("fragment")]
void fragment_main(Vertex input) {
    if((input.flags&1)==0) {
        float alpha=sample_shadow_alpha(input.uv,input.voxel);
        if(alpha<.35)discard;
    }
}
