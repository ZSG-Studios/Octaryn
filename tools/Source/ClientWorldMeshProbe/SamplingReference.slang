#include "WorldRaster.slang"
StructuredBuffer<float4> referenceInputs;
RWStructuredBuffer<float4> referenceOutputs;
[shader("compute")]
[numthreads(1,1,1)]
void sampling_reference(uint3 id:SV_DispatchThreadID) {
    float4 a=referenceInputs[id.x*2],b=referenceInputs[id.x*2+1];
    uint voxel=uint(b.z);
    referenceOutputs[id.x]=is_sprite_voxel(voxel)?sample_sprite_albedo_grad(a.xy,voxel,a.zw,b.xy):
        sample_face_albedo_grad(a.xy,voxel,a.zw,b.xy);
}
