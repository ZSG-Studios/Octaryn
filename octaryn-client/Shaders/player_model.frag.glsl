#version 450

layout(location = 0) in vec4 vWorldPosition;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outVoxel;
layout(location = 3) out vec4 outMaterial;

void main()
{
    outColor = vColor;
    outPosition = vWorldPosition;
    outVoxel = vec4(0.0, 0.0, 0.0, 1.0);
    outMaterial = vec4(0.7, 0.0, 0.02, 0.0);
}
