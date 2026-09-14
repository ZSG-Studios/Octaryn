#include "WorldRaster.slang"
StructuredBuffer<float4> filterInputs;
RWStructuredBuffer<float4> filterOutputs;
SamplerState calibrationSampler;
[shader("compute")]
[numthreads(1,1,1)]
void atlas_filtering(uint3 id:SV_DispatchThreadID) {
    float4 a=filterInputs[id.x*2],b=filterInputs[id.x*2+1];
    uint voxel=uint(b.z),mode=uint(b.w);
    if(mode==3) filterOutputs[id.x]=atlasAlbedo.SampleGrad(calibrationSampler,float3(a.xy,get_index(voxel)),a.zw,b.xy);
    else if(mode==4) filterOutputs[id.x]=atlasAlbedo.SampleGrad(atlasLinear,float3(a.xy,get_index(voxel)),a.zw*.45,b.xy*.45);
    else if(mode==1) filterOutputs[id.x]=sample_sprite_albedo_grad(a.xy,voxel,a.zw,b.xy);
    else if(mode==2) filterOutputs[id.x]=sample_face_tiled_atlas_grad(atlasAlbedo,a.xy,voxel,a.zw,b.xy);
    else filterOutputs[id.x]=sample_face_albedo_grad(a.xy,voxel,a.zw,b.xy);
}
