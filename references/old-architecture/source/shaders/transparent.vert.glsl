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
layout(location = 4) out vec2 vFragment;
layout(location = 5) flat out ivec2 vChunkPosition;
layout(location = 6) flat out uint vChunkSlot;
layout(location = 7) flat out uint vWaterLevel;
layout(location = 8) flat out uint vWaterFace;
layout(location = 9) out float vWaterFill;
layout(location = 10) flat out uint vWaterFlow;

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
    uint waterLevel = 0u;
    uint waterFace = 0u;
    if (draw_uses_face_buffer(DrawFlags))
    {
        uvec2 packed_face = packed_faces[face_index];
        position = get_face_position(packed_face, face_vertex);
        texcoord = get_face_texcoord(packed_face, face_vertex);
        normal = get_face_normal(packed_face);
        voxel = pack_face_vertex_voxel(packed_face, face_vertex);
        waterLevel = get_face_water_level(packed_face);
        waterFace = get_face_is_water(packed_face) ? 1u : 0u;
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
    vWorldPosition.xyz = position + vec3(chunkPosition.x, 0.0, chunkPosition.y);
    vec4 viewPos = get_camera_relative_view_position_from_chunk(View, position, chunkPosition, CameraPosition.xyz);
    vWorldPosition.w = viewPos.z;
    gl_Position = Proj * viewPos;
    vTexcoord = texcoord;
    vVoxel = voxel;
    vFragment = gl_Position.xy / gl_Position.w;
    vFragment = vFragment * 0.5 + 0.5;
    vFragment.y = 1.0 - vFragment.y;
    vChunkPosition = chunkPosition;
    vChunkSlot = chunkSlot;
    vWaterLevel = waterLevel;
    vWaterFace = waterFace;
    vWaterFill = 1.0;
    vWaterFlow = 0u;
}
