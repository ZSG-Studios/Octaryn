#include "VoxelDecode.slang"

struct VoxelRasterInput
{
    uint vertexId : SV_VertexID;
    uint firstVertex : SV_StartVertexLocation;
    uint instanceId : SV_InstanceID;
    uint firstInstance : SV_StartInstanceLocation;
};

uint voxel_probe_absolute_vertex(VoxelRasterInput input)
{
    __target_switch
    {
    // Pinned Slang emits Metal vertex_id without subtracting base_vertex.
    case metal: return input.vertexId;
    default: return input.vertexId + input.firstVertex;
    }
}

struct VoxelRasterOutput
{
    float4 position : SV_Position;
    uint materialId : TEXCOORD0;
};

StructuredBuffer<PackedVoxelQuad16> voxelQuads : register(t0, space0);

static const uint2 OctarynRasterCorners[6] =
{
    uint2(0, 0), uint2(1, 0), uint2(1, 1),
    uint2(0, 0), uint2(1, 1), uint2(0, 1)
};

uint voxel_raster_quad_local_y(PackedVoxelQuad16 quad)
{
    return (quad.pos_size_dir_lod >> 5u) & 31u;
}

uint voxel_raster_quad_width(PackedVoxelQuad16 quad)
{
    return ((quad.pos_size_dir_lod >> 15u) & 31u) + 1u;
}

uint voxel_raster_quad_height(PackedVoxelQuad16 quad)
{
    return ((quad.pos_size_dir_lod >> 20u) & 31u) + 1u;
}

float2 voxel_probe_clip_position(PackedVoxelQuad16 quad, uint2 corner)
{
    float x = float(voxel_quad_local_x(quad)) + float(corner.x * voxel_raster_quad_width(quad));
    float y = float(voxel_raster_quad_local_y(quad)) + float(corner.y * voxel_raster_quad_height(quad));
    return float2((x / 16.0) - 1.0, 1.0 - (y / 16.0));
}

[shader("vertex")]
VoxelRasterOutput vertex_main(VoxelRasterInput input)
{
    PackedVoxelQuad16 quad = voxelQuads[input.instanceId + input.firstInstance];
    uint2 corner = OctarynRasterCorners[voxel_probe_absolute_vertex(input) % 6u];

    VoxelRasterOutput output;
    output.position = float4(voxel_probe_clip_position(quad, corner), 0.5, 1.0);
    output.materialId = quad.material_flags & 0x000fffffu;
    return output;
}

[shader("fragment")]
float4 fragment_main(VoxelRasterOutput input) : SV_Target0
{
    float materialTint = 0.2 + float(input.materialId & 15u) * 0.05;
    return float4(materialTint, 0.75, 0.35, 1.0);
}
