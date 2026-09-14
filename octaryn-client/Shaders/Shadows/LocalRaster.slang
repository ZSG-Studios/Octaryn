#include "../Materials/MaterialSampling.slang"
#include "../Voxel/WorldGeometry.slang"
StructuredBuffer<uint4> faces;
StructuredBuffer<FluidFace> fluidFaces;
uniform uint fluidBase;
uniform float4 localShadowPosition,localShadowProjection;
uniform float4 localRight,localUp,localForward;
uniform uint localShadowVoxel;
struct LocalVertex {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    nointerpolation uint voxel:TEXCOORD1;
    nointerpolation uint flags:TEXCOORD2;
    nointerpolation uint source:TEXCOORD3;
};
[shader("vertex")]
LocalVertex vertex_main(uint vertex:SV_VertexID,uint localInstance:SV_InstanceID,uint baseInstance:SV_StartInstanceLocation) {
    uint instance=localInstance+baseInstance;
    uint4 face=faces[instance];FluidFace fluid;fluid.heights=0;fluid.flow=0;
    if((blockMaterials[face.w&65535].flags&4)!=0)fluid=fluidFaces[instance-fluidBase];
    WorldGeometryVertex geometry=world_geometry_corner(vertex,face,fluid);
    float3 p=float3(asint(face.xyz))+geometry.offset-localShadowPosition.xyz;
    float z=dot(p,localForward.xyz);
    LocalVertex result;
    result.position=float4(dot(p,localRight.xyz),dot(p,localUp.xyz),z*localShadowProjection.x-localShadowProjection.y,z);
    result.uv=geometry.uv;result.voxel=geometry.voxel;result.flags=geometry.flags;
    result.source=localShadowVoxel!=0 && all(float3(asint(face.xyz))==floor(localShadowPosition.xyz))?1:0;
    return result;
}
[shader("fragment")]
void fragment_main(LocalVertex input) {
    if(input.source!=0)discard;
    if((input.flags&1)==0) {
        float alpha=is_sprite_voxel(input.voxel)?sample_sprite_atlas(atlasAlbedo,input.uv,input.voxel).a:
            atlasAlbedo.SampleLevel(atlasCutout,float3(input.uv,get_index(input.voxel)),0).a;
        if(alpha<.35)discard;
    }
}
