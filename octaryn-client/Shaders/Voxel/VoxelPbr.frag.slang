#include "../Materials/MaterialSampling.slang"
#include "../Materials/PbrEvaluate.slang"

struct VoxelPbrInput
{
    float4 position : SV_Position;
    float3 worldNormal : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

float4 main(VoxelPbrInput input) : SV_Target0
{
    float3 normal = normalize(input.worldNormal);
    float lighting = saturate(dot(normal, normalize(float3(0.35, 0.75, 0.55)))) * 0.7 + 0.3;
    float materialTint = 0.75 + float(input.materialId & 7u) * 0.03;
    float3 albedo = octaryn_default_albedo() * materialTint;
    return float4(octaryn_pbr_identity(albedo) * lighting, 1.0);
}
