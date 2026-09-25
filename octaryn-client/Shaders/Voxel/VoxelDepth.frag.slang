struct VoxelDepthInput
{
    float4 position : SV_Position;
};

float4 main(VoxelDepthInput input) : SV_Target0
{
    return float4(input.position.zzz, 1.0);
}
