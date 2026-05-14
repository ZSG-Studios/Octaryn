#version 450

#include "voxel_common.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FaceBuffer
{
    uvec2 packed_faces[];
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

void main()
{
    uint voxel = inVoxel;
    uint face_vertex = uint(gl_VertexIndex) % 6u;
    uint face_index = uint(gl_InstanceIndex) + (uint(gl_VertexIndex) / 6u);
    vec3 position = draw_uses_face_buffer(DrawFlags)
                  ? get_face_position(packed_faces[face_index], face_vertex)
                  : get_position(voxel);
    gl_Position = Proj * get_camera_relative_view_position_from_chunk(View, position, ChunkPosition, CameraPosition.xyz);
}
