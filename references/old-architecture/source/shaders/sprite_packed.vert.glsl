#version 450

#include "voxel_common.glsl"

layout(std430, set = 0, binding = 0) readonly buffer SpriteBuffer
{
    uint packed_sprites[];
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
layout(location = 4) flat out ivec2 vChunkPosition;
layout(location = 5) flat out uint vChunkSlot;

void main()
{
    uint voxel = packed_sprites[uint(gl_VertexIndex)];
    uint chunkSlot = kInvalidDescriptorIndex;
    ivec2 chunkPosition = ChunkPosition;
    if (draw_uses_descriptor_buffer(DrawFlags))
    {
        chunkSlot = ChunkDescriptorIndex;
        chunkPosition = descriptors[chunkSlot].ChunkPosition;
    }
    vec3 position = get_position(voxel);

    vNormal = get_normal(voxel);
    vec4 viewPos = get_camera_relative_view_position_from_chunk(View, position, chunkPosition, CameraPosition.xyz);
    vWorldPosition.xyz = vec3(float(chunkPosition.x) - CameraPosition.x,
                              -CameraPosition.y,
                              float(chunkPosition.y) - CameraPosition.z) + position;
    vWorldPosition.w = viewPos.z;
    gl_Position = Proj * viewPos;
    vTexcoord = get_texcoord(voxel);
    vVoxel = voxel;
    vChunkPosition = chunkPosition;
    vChunkSlot = chunkSlot;
}
