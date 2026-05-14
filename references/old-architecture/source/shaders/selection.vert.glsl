#version 450

#include "voxel_common.glsl"

layout(set = 1, binding = 0) uniform TransformUniforms { mat4 Transform; };
layout(set = 1, binding = 1) uniform BlockUniforms { ivec3 BlockPosition; };
layout(set = 1, binding = 2) uniform SelectionUniforms {
    float Brightness;
    float Alpha;
    float AtlasIndex;
    float UseTexture;
    float Offset;
    float Bias;
    uint HitFace;
    uint FaceOnly;
};

layout(location = 0) out vec2 vTexcoord;

void main()
{
    uint vertexId = uint(gl_VertexIndex);
    uint face = FaceOnly != 0u ? HitFace : vertexId / 6u;
    uint corner = kQuadIndices[vertexId % 6u];
    uvec2 uv_bits = get_face_uv_bits(corner);
    float u = float(uv_bits.x);
    float v = float(uv_bits.y);

    vec3 pos = vec3(0.0);
    if (face == kDirectionNorth) { pos = vec3(u, v, 1.0); }
    else if (face == kDirectionSouth) { pos = vec3(1.0 - u, v, 0.0); }
    else if (face == kDirectionEast) { pos = vec3(1.0, v, 1.0 - u); }
    else if (face == kDirectionWest) { pos = vec3(0.0, v, u); }
    else if (face == kDirectionUp) { pos = vec3(u, 1.0, 1.0 - v); }
    else if (face == kDirectionDown) { pos = vec3(u, 0.0, v); }

    // Selection box is slightly larger than the block
    float selection_offset = Offset + 0.0005;
    float off_u = (u > 0.5) ? selection_offset : -selection_offset;
    float off_v = (v > 0.5) ? selection_offset : -selection_offset;
    vec3 expand = vec3(0.0);

    if (face == kDirectionNorth) { expand = vec3(off_u, off_v, selection_offset); }
    else if (face == kDirectionSouth) { expand = vec3(-off_u, off_v, -selection_offset); }
    else if (face == kDirectionEast) { expand = vec3(selection_offset, off_v, -off_u); }
    else if (face == kDirectionWest) { expand = vec3(-selection_offset, off_v, off_u); }
    else if (face == kDirectionUp) { expand = vec3(off_u, selection_offset, -off_v); }
    else if (face == kDirectionDown) { expand = vec3(off_u, -selection_offset, off_v); }

    vec3 position = pos + expand + vec3(BlockPosition);
    gl_Position = Transform * vec4(position, 1.0);
    gl_Position.z -= Bias;
    vTexcoord = vec2(u, v);
}
