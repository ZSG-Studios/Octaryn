#version 450

#include "voxel_common.glsl"

struct WaterVertex
{
    vec4 Position;
    vec4 Texcoord;
    uvec4 Data;
};

layout(std430, set = 0, binding = 0) readonly buffer WaterBuffer
{
    WaterVertex water_vertices[];
};

layout(std430, set = 0, binding = 1) readonly buffer DescriptorBuffer
{
    ChunkDescriptor descriptors[];
};

layout(set = 1, binding = 0) uniform ProjUniforms { mat4 Proj; };
layout(set = 1, binding = 1) uniform ViewUniforms { mat4 View; };
layout(set = 1, binding = 2) uniform ChunkUniforms {
    ivec2 ChunkPosition;
    uint ChunkDescriptorIndex;
    uint DrawFlags;
};
layout(set = 1, binding = 3) uniform CameraUniforms { vec4 CameraPosition; };

layout(location = 0) out vec4 vWorldPosition;
layout(location = 1) flat out vec3 vNormal;
layout(location = 2) out vec3 vTexcoord;
layout(location = 3) flat out uint vVoxel;
layout(location = 7) flat out uint vWaterLevel;
layout(location = 8) flat out uint vWaterFace;
layout(location = 9) out float vWaterFill;
layout(location = 10) flat out uint vWaterFlow;

void main()
{
    WaterVertex vertex = water_vertices[uint(gl_VertexIndex)];
    uint direction = vertex.Data.x;
    uint water_level = vertex.Data.y;
    uint atlas_index = vertex.Data.z;
    uint voxel = 0u;
    voxel |= direction << DIRECTION_OFFSET;
    voxel |= atlas_index << INDEX_OFFSET;

    ivec2 chunkPosition = ChunkPosition;
    if (draw_uses_descriptor_buffer(DrawFlags) && ChunkDescriptorIndex != kInvalidDescriptorIndex)
    {
        chunkPosition = descriptors[ChunkDescriptorIndex].ChunkPosition;
    }

    vec3 position = vertex.Position.xyz;
    vNormal = kNormals[direction];
    vWorldPosition.xyz = position + vec3(chunkPosition.x, 0.0, chunkPosition.y);
    vec4 viewPos = get_camera_relative_view_position_from_chunk(View, position, chunkPosition, CameraPosition.xyz);
    vWorldPosition.w = viewPos.z;
    gl_Position = Proj * viewPos;
    vTexcoord = vertex.Texcoord.xyz;
    vVoxel = voxel;
    vWaterLevel = water_level;
    vWaterFace = 1u;
    vWaterFill = clamp(vertex.Texcoord.w, 0.0, 1.0);
    vWaterFlow = vertex.Data.w;
}
