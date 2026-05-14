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

void main()
{
    uint face_vertex = uint(gl_VertexIndex) % 6u;
    uint face_index = uint(gl_InstanceIndex) + (uint(gl_VertexIndex) / 6u);
    uvec2 packed_face = packed_faces[face_index];
    vec3 position = get_face_position(packed_face, face_vertex);
    ivec2 chunkPosition = ChunkPosition;
    if (draw_uses_descriptor_buffer(DrawFlags))
    {
        chunkPosition = descriptors[get_face_chunk_slot(packed_face)].ChunkPosition;
    }
    gl_Position = Proj * get_camera_relative_view_position_from_chunk(View, position, chunkPosition, CameraPosition.xyz);
}
