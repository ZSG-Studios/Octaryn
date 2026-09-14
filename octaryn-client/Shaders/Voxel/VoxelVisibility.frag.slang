struct VoxelVisibilityInput
{
    float4 position : SV_Position;
    float3 worldNormal : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

uint main(VoxelVisibilityInput input) : SV_Target0
{
    uint depthClass = (uint)saturate(input.position.z) & 0xffu;
    return (input.materialId & 0x000fffffu) | (depthClass << 20u);
}
