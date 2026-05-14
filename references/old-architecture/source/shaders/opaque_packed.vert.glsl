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
    uvec2 packed_face = packed_faces[face_index];
    vec3 position = get_face_position(packed_face, face_vertex);
    vec3 texcoord = get_face_texcoord(packed_face, face_vertex);
    vec3 normal = get_face_normal(packed_face);
    uint voxel = pack_face_vertex_voxel(packed_face, face_vertex);
    ivec2 chunkPosition = ChunkPosition;
    uint chunkSlot = kInvalidDescriptorIndex;
    if (draw_uses_descriptor_buffer(DrawFlags))
    {
        chunkSlot = get_face_chunk_slot(packed_face);
        chunkPosition = descriptors[chunkSlot].ChunkPosition;
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
