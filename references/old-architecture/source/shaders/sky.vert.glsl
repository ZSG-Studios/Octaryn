#version 450

#include "voxel_common.glsl"

layout(set = 1, binding = 0) uniform ProjUniforms { mat4 Proj; };
layout(set = 1, binding = 1) uniform ViewUniforms { mat4 View; };

layout(location = 0) out vec3 vLocalPosition;

void main()
{
    mat4 view = View;
    view[3][0] = 0.0;
    view[3][1] = 0.0;
    view[3][2] = 0.0;
    vLocalPosition = get_cube_position(uint(gl_VertexIndex));
    gl_Position = Proj * (view * vec4(vLocalPosition, 1.0));
    gl_Position.z = gl_Position.w;
}
