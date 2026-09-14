#include "VoxelDecode.slang"

struct VoxelFrameConstants
{
    float4x4 viewProjection;
    int3 columnOrigin;
    uint chunkY;
};

struct VoxelFacePullInput
{
    uint vertexId : SV_VertexID;
    uint instanceId : SV_InstanceID;
};

struct VoxelFacePullOutput
{
    float4 position : SV_Position;
    float3 worldNormal : TEXCOORD0;
    uint materialId : TEXCOORD1;
};

StructuredBuffer<PackedVoxelQuad16> voxelQuads : register(t0, space0);
ConstantBuffer<VoxelFrameConstants> voxelFrame : register(b0, space0);

static const uint2 OctarynQuadCorners[6] =
{
    uint2(0, 0), uint2(1, 0), uint2(1, 1),
    uint2(0, 0), uint2(1, 1), uint2(0, 1)
};

uint voxel_quad_local_y(PackedVoxelQuad16 quad)
{
    return (quad.pos_size_dir_lod >> 5u) & 31u;
}

uint voxel_quad_local_z(PackedVoxelQuad16 quad)
{
    return (quad.pos_size_dir_lod >> 10u) & 31u;
}

uint voxel_quad_width(PackedVoxelQuad16 quad)
{
    return ((quad.pos_size_dir_lod >> 15u) & 31u) + 1u;
}

uint voxel_quad_height(PackedVoxelQuad16 quad)
{
    return ((quad.pos_size_dir_lod >> 20u) & 31u) + 1u;
}

float3 voxel_face_normal(uint faceDir)
{
    if (faceDir == 0u) return float3(1.0, 0.0, 0.0);
    if (faceDir == 1u) return float3(-1.0, 0.0, 0.0);
    if (faceDir == 2u) return float3(0.0, 1.0, 0.0);
    if (faceDir == 3u) return float3(0.0, -1.0, 0.0);
    if (faceDir == 4u) return float3(0.0, 0.0, 1.0);
    return float3(0.0, 0.0, -1.0);
}

float3 voxel_corner_offset(uint faceDir, uint2 corner, uint width, uint height)
{
    float u = float(corner.x * width);
    float v = float(corner.y * height);
    if (faceDir < 2u) return float3(0.0, u, v);
    if (faceDir < 4u) return float3(u, 0.0, v);
    return float3(u, v, 0.0);
}

VoxelFacePullOutput main(VoxelFacePullInput input)
{
    PackedVoxelQuad16 quad = voxelQuads[input.instanceId];
    uint faceDir = voxel_quad_face_dir(quad);
    uint2 corner = OctarynQuadCorners[input.vertexId % 6u];
    float3 localBase = float3(
        float(voxel_quad_local_x(quad)),
        float(voxelFrame.chunkY * OctarynChunkWidthBlocks + voxel_quad_local_y(quad)),
        float(voxel_quad_local_z(quad)));
    float3 worldPosition = float3(voxelFrame.columnOrigin) + localBase + voxel_corner_offset(faceDir, corner, voxel_quad_width(quad), voxel_quad_height(quad));

    VoxelFacePullOutput output;
    output.position = mul(voxelFrame.viewProjection, float4(worldPosition, 1.0));
    output.worldNormal = voxel_face_normal(faceDir);
    output.materialId = quad.material_flags & 0x000fffffu;
    return output;
}
