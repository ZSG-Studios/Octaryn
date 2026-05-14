#version 450

#include "voxel_common.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FaceBuffer
{
    uvec2 packed_faces[];
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

layout(location = 0) in uint inVoxel;

layout(location = 0) out vec4 vWorldPosition;
layout(location = 1) flat out vec3 vNormal;
layout(location = 2) out vec3 vTexcoord;
layout(location = 3) flat out uint vVoxel;
layout(location = 4) flat out ivec2 vChunkPosition;
layout(location = 5) flat out uint vChunkSlot;

void main()
{
    uint face_vertex = uint(gl_VertexIndex) % 6u;
    uint face_index = uint(gl_InstanceIndex) + (uint(gl_VertexIndex) / 6u);
    uint voxel = inVoxel;
    vec3 position = vec3(0.0);
    vec3 texcoord = vec3(0.0);
    vec3 normal = vec3(0.0);
    ivec2 chunkPosition = ChunkPosition;
    uint chunkSlot = kInvalidDescriptorIndex;
    if (draw_uses_face_buffer(DrawFlags))
    {
        uvec2 packed_face = packed_faces[face_index];
        position = get_face_position(packed_face, face_vertex);
        texcoord = get_face_texcoord(packed_face, face_vertex);
        normal = get_face_normal(packed_face);
        voxel = pack_face_vertex_voxel(packed_face, face_vertex);
        if (draw_uses_descriptor_buffer(DrawFlags))
        {
            chunkSlot = get_face_chunk_slot(packed_face);
            chunkPosition = descriptors[chunkSlot].ChunkPosition;
        }
    }
    else
    {
        position = get_position(voxel);
        texcoord = get_texcoord(voxel);
        normal = get_normal(voxel);
        if (draw_uses_descriptor_buffer(DrawFlags) && ChunkDescriptorIndex != kInvalidDescriptorIndex)
        {
            chunkSlot = ChunkDescriptorIndex;
            chunkPosition = descriptors[ChunkDescriptorIndex].ChunkPosition;
        }
    }
    vNormal = normal;
    vec4 viewPos = get_camera_relative_view_position_from_chunk(View, position, chunkPosition, CameraPosition.xyz);
    vWorldPosition.xyz = vec3(float(chunkPosition.x) - CameraPosition.x,
                              -CameraPosition.y,
                              float(chunkPosition.y) - CameraPosition.z) + position;
    vWorldPosition.w = viewPos.z;
    gl_Position = Proj * viewPos;
    vTexcoord = texcoord;
    vVoxel = voxel;
    vChunkPosition = chunkPosition;
    vChunkSlot = chunkSlot;
}
