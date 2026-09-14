StructuredBuffer<uint4> faces;
#include "../Materials/MaterialSampling.slang"
#include "../Materials/LabPbr.slang"
#include "../Materials/TerrainParallax.slang"
#include "../Materials/PbrEvaluate.slang"
#include "WorldGeometry.slang"
uniform float4 cameraPosition;
uniform float4 cameraRight;
uniform float4 cameraUp;
uniform float4 cameraForward;
uniform float4 projection;
uniform float4 lightDirectionSky;
uniform float4 worldTime;
uniform float4 drawSettings; // first fluid face, fog distance, temporal mip bias, reserved
uniform float4 materialSettings; // PBR, POM, original skylight floor, ambient strength
#include "WorldFluidTypes.slang"
StructuredBuffer<FluidFace> fluidFaces;
StructuredBuffer<uint> patches;
#include "../Materials/SpriteSampling.slang"
#include "WorldPatches.slang"
struct VertexOutput {
    float4 position : SV_Position;
    float3 relative : TEXCOORD0;
    float2 uv : TEXCOORD1;
    nointerpolation uint voxel : TEXCOORD2;
    nointerpolation uint flags : TEXCOORD3;
    nointerpolation float4 fluid : TEXCOORD4;
    float fill : TEXCOORD5;
};
struct SurfaceVertexOutput {
    float4 position : SV_Position;
    float3 relative : TEXCOORD0;
    float2 uv : TEXCOORD1;
    nointerpolation uint voxel : TEXCOORD2;
    nointerpolation uint flags : TEXCOORD3;
};
SurfaceVertexOutput world_surface_output(VertexOutput value) {
    SurfaceVertexOutput output;
    output.position=value.position;output.relative=value.relative;output.uv=value.uv;
    output.voxel=value.voxel;output.flags=value.flags;
    return output;
}
VertexOutput world_vertex_coordinate_with_fluid(uint vertex,uint4 face,float2 coordinate,FluidFace fluid) {
    WorldGeometryVertex geometry=world_geometry_vertex(vertex,face,coordinate,fluid);
    VertexOutput output;
    output.uv=geometry.uv;output.fluid=geometry.fluid;output.fill=geometry.fill;
    float3 offset=geometry.offset;
    // Preserve the original chunk-relative arithmetic before adding thin geometry.
    int3 block=asint(face.xyz);
    int2 chunk=block.xz & int2(~31);
    precise float3 origin=float3(chunk.x,0,chunk.y)-cameraPosition.xyz;
    precise float3 local=float3(block.x-chunk.x,block.y,block.z-chunk.y)+offset;
    output.relative=origin+local;
    float z=dot(output.relative,cameraForward.xyz);
    // RHI uses a negative Vulkan viewport height; clip Y stays up.
    output.position=float4(dot(output.relative,cameraRight.xyz)*projection.x,
        dot(output.relative,cameraUp.xyz)*projection.y,z*projection.z-projection.w,z);
    output.position.xy+=float2(cameraRight.w,cameraUp.w)*z;
    output.voxel=geometry.voxel;
    output.flags=geometry.flags;
    return output;
}
VertexOutput world_vertex_coordinate(uint vertex,uint instance,uint4 face,float2 coordinate) {
    FluidFace fluid;fluid.heights=0;fluid.flow=0;
    if((blockMaterials[face.w&65535].flags&4)!=0)
        fluid=fluidFaces[instance-uint(drawSettings.x)];
    return world_vertex_coordinate_with_fluid(vertex,face,coordinate,fluid);
}
VertexOutput world_vertex(uint vertex,uint instance) {
    uint4 face=faces[instance];uint packed=face.w;
    float2 extent=float2(((packed>>20)&31)+1,((packed>>25)&31)+1);
    return world_vertex_coordinate(vertex,instance,face,world_quad_corner(vertex)*extent);
}
VertexOutput world_patch_vertex(uint vertex,uint localInstance,uint baseInstance) {
    uint instance=localInstance+baseInstance;
    uint patch=patches[instance],faceIndex=patch>>6;
    uint4 face=faces[faceIndex];uint packed=face.w;
    uint2 extent=uint2(((packed>>20)&31)+1,((packed>>25)&31)+1);
    return world_vertex_coordinate(vertex,faceIndex,face,world_patch_corner(extent,patch&63,vertex));
}
[shader("vertex")]
SurfaceVertexOutput vertex_main(uint vertex : SV_VertexID, uint localInstance : SV_InstanceID,
    uint baseInstance : SV_StartInstanceLocation) {
    return world_surface_output(world_patch_vertex(vertex,localInstance,baseInstance));
}
[shader("vertex")]
VertexOutput forward_vertex_main(uint vertex : SV_VertexID, uint localInstance : SV_InstanceID,
    uint baseInstance : SV_StartInstanceLocation) {
    return world_patch_vertex(vertex,localInstance,baseInstance);
}
#include "WorldFluidShade.slang"
struct GBufferOutput {
    float4 color:SV_Target0;
    float4 relative:SV_Target1;
    float4 voxel:SV_Target2;
    float4 material:SV_Target3;
};
[shader("fragment")]
GBufferOutput fragment_main(SurfaceVertexOutput input) {
    float gradientScale=exp2(drawSettings.z);
    float2 uv=input.uv,dx=ddx(uv)*gradientScale,dy=ddy(uv)*gradientScale;
    float distance=length(input.relative);
    if(materialSettings.x!=0 && materialSettings.y!=0 && distance<kTerrainPomDistance &&
        terrain_face_edge_weight(atlasNormal,uv)>.001)
        uv=apply_terrain_parallax(atlasNormal,uv,dx,dy,-input.relative/max(distance,.001),distance,input.voxel,input.position.xy).uv;
    float4 albedo=is_sprite_voxel(input.voxel)?sample_sprite_albedo_grad(uv,input.voxel,dx,dy):
        sample_face_albedo_grad(uv,input.voxel,dx,dy);
    if((input.flags&1)==0 && albedo.a<.35) discard;
    GBufferOutput output;
    output.color=float4(albedo.rgb,original_ambient_visibility(get_normal(input.voxel),is_sprite_voxel(input.voxel),materialSettings.z,lightDirectionSky.w));
    output.relative=float4(input.relative,dot(input.relative,cameraForward.xyz));
    uint voxel=input.voxel;output.voxel=float4(voxel&255,(voxel>>8)&255,(voxel>>16)&255,voxel>>24)/255.0;
    output.material=float4(.8,0,.04,0);
    if(materialSettings.x!=0) {
        float4 normalSample=is_sprite_voxel(input.voxel)?sample_sprite_atlas(atlasNormal,uv,input.voxel):
            sample_face_tiled_atlas_grad(atlasNormal,uv,input.voxel,dx,dy);
        float4 specularSample=is_sprite_voxel(input.voxel)?sample_sprite_atlas(atlasSpecular,uv,input.voxel):
            sample_face_tiled_atlas_grad(atlasSpecular,uv,input.voxel,dx,dy);
        PbrMaterial material=decode_labpbr_material(normalSample,specularSample,get_normal(input.voxel),input.voxel);
        output.material=float4(material.roughness,material.metallic,material.f0,material.emission);
    }
    return output;
}
[shader("fragment")]
float4 forward_main(VertexOutput input,bool front:SV_IsFrontFace):SV_Target0 {
    float4 color;
    if((input.flags&4)!=0) color=shadeOriginalFluid(input,front);
    else color=fluidSampleAtlas(atlasAlbedo,input.uv,input.voxel);
    color.rgb=apply_scene_fog(color.rgb,input.relative,lightDirectionSky.w,worldTime.y,drawSettings.y>0,drawSettings.y);
    return color;
}
