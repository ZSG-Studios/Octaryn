#version 450

#include "voxel_common.glsl"

struct PlayerVertex
{
    vec4 Position;
    vec4 Color;
};

layout(set = 1, binding = 0) uniform ProjUniforms { mat4 Proj; };
layout(set = 1, binding = 1) uniform ViewUniforms { mat4 View; };
layout(set = 1, binding = 2) uniform CameraUniforms { vec4 CameraPosition; };
layout(set = 1, binding = 3) uniform PlayerVertexUniforms
{
    PlayerVertex Vertices[64];
};

layout(location = 0) out vec4 vWorldPosition;
layout(location = 1) out vec4 vColor;

void main()
{
    PlayerVertex vertex = Vertices[gl_VertexIndex];
    vec4 viewPos = get_camera_relative_view_position(View, vertex.Position.xyz, CameraPosition.xyz);
    gl_Position = Proj * viewPos;
    vWorldPosition = vec4(vertex.Position.xyz - CameraPosition.xyz, viewPos.z);
    vColor = vertex.Color;
}
