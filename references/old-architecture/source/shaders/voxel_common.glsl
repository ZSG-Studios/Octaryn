#include "voxel.inc"

const uint kHiddenBlockCapacity = 32u;
const uint kDirectionNorth = 0u;
const uint kDirectionSouth = 1u;
const uint kDirectionEast = 2u;
const uint kDirectionWest = 3u;
const uint kDirectionUp = 4u;
const uint kDirectionDown = 5u;
const uint kDirectionCount = 6u;
const uint kInvalidDescriptorIndex = 0xFFFFFFFFu;
const uint kDrawFlagSprite = 0x1u;
const uint kDrawFlagUseFaceBuffer = 0x2u;
const uint kDrawFlagUseDescriptorBuffer = 0x4u;

const vec3 kNormals[12] = vec3[12](
    vec3( 0.0, 0.0, 1.0), // North
    vec3( 0.0, 0.0,-1.0), // South
    vec3( 1.0, 0.0, 0.0), // East
    vec3(-1.0, 0.0, 0.0), // West
    vec3( 0.0, 1.0, 0.0), // Up
    vec3( 0.0,-1.0, 0.0), // Down
    normalize(vec3(-1.0, 0.6,  1.0)),
    normalize(vec3( 1.0, 0.6, -1.0)),
    normalize(vec3(-1.0, 0.6, -1.0)),
    normalize(vec3( 1.0, 0.6,  1.0)),
    vec3( 0.0, 1.0, 0.0),
    vec3( 0.0,-1.0, 0.0)
);

const uint kIndices[36] = uint[36](
    0, 1, 2, 0, 2, 3, // South
    5, 4, 7, 5, 7, 6, // North
    1, 5, 6, 1, 6, 2, // East
    4, 0, 3, 4, 3, 7, // West
    3, 2, 6, 3, 6, 7, // Up
    4, 5, 1, 4, 1, 0  // Down
);

const uint kQuadIndices[6] = uint[6](0u, 1u, 3u, 0u, 3u, 2u);

const vec3 kPositions[10] = vec3[10](
    vec3(-0.5, -0.5, -0.5),
    vec3( 0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5),
    vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5,  0.5),
    vec3( 0.5, -0.5,  0.5),
    vec3( 0.5,  0.5,  0.5),
    vec3(-0.5,  0.5,  0.5),
    vec3(-0.5, -0.5, -0.5), // Sprite
    vec3( 0.5,  0.5,  0.5)  // Sprite
);

struct ChunkDescriptor
{
    ivec2 ChunkPosition;
    uint SlotId;
    uint Flags;
    uint FaceOffsets[4];
    uint FaceCounts[4];
    uint SkylightOffset;
    uint SkylightCount;
};

const uint kFaceXBits = 5u;
const uint kFaceYBits = 8u;
const uint kFaceZBits = 5u;
const uint kFaceDirectionBits = 3u;
const uint kFaceSpanUBits = 8u;
const uint kFaceSpanVBits = 8u;
const uint kFaceIndexBits = 6u;
const uint kFaceOcclusionBits = 1u;
const uint kFaceChunkSlotBits = 13u;
const uint kFaceWaterLevelBits = 3u;
const uint kFaceWaterFlagBits = 1u;
const uint kFaceWaterBaseHeightBits = 3u;

const uint kFaceXOffset = 0u;
const uint kFaceYOffset = kFaceXOffset + kFaceXBits;
const uint kFaceZOffset = kFaceYOffset + kFaceYBits;
const uint kFaceDirectionOffset = kFaceZOffset + kFaceZBits;
const uint kFaceSpanUOffset = kFaceDirectionOffset + kFaceDirectionBits;
const uint kFaceSpanVOffset = kFaceSpanUOffset + kFaceSpanUBits;
const uint kFaceIndexOffset = kFaceSpanVOffset + kFaceSpanVBits;
const uint kFaceOcclusionOffset = kFaceIndexOffset + kFaceIndexBits;
const uint kFaceChunkSlotOffset = kFaceOcclusionOffset + kFaceOcclusionBits;
const uint kFaceWaterLevelOffset = kFaceChunkSlotOffset + kFaceChunkSlotBits;
const uint kFaceWaterFlagOffset = kFaceWaterLevelOffset + kFaceWaterLevelBits;
const uint kFaceWaterBaseHeightOffset = kFaceWaterFlagOffset + kFaceWaterFlagBits;

const uint kFaceXMask = (1u << kFaceXBits) - 1u;
const uint kFaceYMask = (1u << kFaceYBits) - 1u;
const uint kFaceZMask = (1u << kFaceZBits) - 1u;
const uint kFaceDirectionMask = (1u << kFaceDirectionBits) - 1u;
const uint kFaceSpanUMask = (1u << kFaceSpanUBits) - 1u;
const uint kFaceSpanVMask = (1u << kFaceSpanVBits) - 1u;
const uint kFaceIndexMask = (1u << kFaceIndexBits) - 1u;
const uint kFaceOcclusionMask = (1u << kFaceOcclusionBits) - 1u;
const uint kFaceChunkSlotMask = (1u << kFaceChunkSlotBits) - 1u;
const uint kFaceWaterLevelMask = (1u << kFaceWaterLevelBits) - 1u;
const uint kFaceWaterFlagMask = (1u << kFaceWaterFlagBits) - 1u;
const uint kFaceWaterBaseHeightMask = (1u << kFaceWaterBaseHeightBits) - 1u;

const float kFaceInflation = 0.0;

uint face_extract(uvec2 packed_face, uint offset, uint mask)
{
    const uint bits = findMSB(mask) + 1u;
    if (offset >= 32u)
    {
        return (packed_face.y >> (offset - 32u)) & mask;
    }
    if (offset + bits <= 32u)
    {
        return (packed_face.x >> offset) & mask;
    }

    const uint low_bits = 32u - offset;
    const uint high_bits = bits - low_bits;
    const uint low_mask = (1u << low_bits) - 1u;
    const uint high_mask = (1u << high_bits) - 1u;
    const uint low = (packed_face.x >> offset) & low_mask;
    const uint high = packed_face.y & high_mask;
    return (high << low_bits) | low;
}

uint get_direction(uint voxel)
{
    return (voxel >> DIRECTION_OFFSET) & DIRECTION_MASK;
}

vec3 get_normal(uint voxel)
{
    uint direction = get_direction(voxel);
    uint atlas_index = (voxel >> INDEX_OFFSET) & INDEX_MASK;
    if (atlas_index >= 17u && atlas_index <= 23u && direction >= kDirectionCount)
    {
        return kNormals[direction - kDirectionCount];
    }
    return kNormals[direction];
}

uvec3 get_block_position(uint voxel)
{
    return uvec3((voxel >> X_OFFSET) & X_MASK,
                 (voxel >> Y_OFFSET) & Y_MASK,
                 (voxel >> Z_OFFSET) & Z_MASK);
}

vec3 get_position(uint voxel)
{
    vec3 position = vec3(get_block_position(voxel));
    float u = float((voxel >> U_OFFSET) & U_MASK);
    float v = float((voxel >> V_OFFSET) & V_MASK);
    uint direction = get_direction(voxel);
    uint atlas_index = (voxel >> INDEX_OFFSET) & INDEX_MASK;

    if (direction == kDirectionNorth) { position += vec3(u, v, 1.0); }
    else if (direction == kDirectionSouth) { position += vec3((1.0 - u), v, 0.0); }
    else if (direction == kDirectionEast) { position += vec3(1.0, v, (1.0 - u)); }
    else if (direction == kDirectionWest) { position += vec3(0.0, v, u); }
    else if (direction == kDirectionUp) { position += vec3(u, 1.0, (1.0 - v)); }
    else if (direction == kDirectionDown) { position += vec3(u, 0.0, v); }
    else if (direction >= kDirectionCount)
    {
        bool torch_sprite = atlas_index >= 17u && atlas_index <= 23u;
        if (torch_sprite)
        {
            uint face = direction - kDirectionCount;
            float min_x = 7.0 / 16.0;
            float max_x = 9.0 / 16.0;
            float min_y = 0.0;
            float max_y = 10.0 / 16.0;
            float min_z = 7.0 / 16.0;
            float max_z = 9.0 / 16.0;
            if (face == kDirectionNorth) { position += vec3(mix(min_x, max_x, u), mix(min_y, max_y, v), max_z); }
            else if (face == kDirectionSouth) { position += vec3(mix(max_x, min_x, u), mix(min_y, max_y, v), min_z); }
            else if (face == kDirectionEast) { position += vec3(max_x, mix(min_y, max_y, v), mix(max_z, min_z, u)); }
            else if (face == kDirectionWest) { position += vec3(min_x, mix(min_y, max_y, v), mix(min_z, max_z, u)); }
            else if (face == kDirectionUp) { position += vec3(mix(min_x, max_x, u), max_y, mix(max_z, min_z, v)); }
            else { position += vec3(mix(min_x, max_x, u), min_y, mix(min_z, max_z, v)); }
            return position;
        }
        if (direction == kDirectionCount || direction == kDirectionCount + 1u)
        {
            position += vec3(1.0 - u, 1.0 - v, 1.0 - u);
        }
        else
        {
            position += vec3(1.0 - u, 1.0 - v, u);
        }
    }
    return position;
}

vec3 get_cube_position(uint vertex_id)
{
    return kPositions[kIndices[vertex_id % 36u]];
}

vec3 get_texcoord(uint voxel)
{
    float u = float((voxel >> U_OFFSET) & U_MASK);
    float v = float((voxel >> V_OFFSET) & V_MASK);
    uint direction = get_direction(voxel);
    uint atlas_index = (voxel >> INDEX_OFFSET) & INDEX_MASK;
    if (atlas_index >= 17u && atlas_index <= 23u && direction >= kDirectionCount)
    {
        uint face = direction - kDirectionCount;
        vec4 uv = vec4(7.0, 6.0, 9.0, 16.0) / 16.0;
        if (face == kDirectionUp)
        {
            uv = vec4(7.0, 6.0, 9.0, 8.0) / 16.0;
        }
        else if (face == kDirectionDown)
        {
            uv = vec4(7.0, 13.0, 9.0, 15.0) / 16.0;
        }
        return vec3(mix(uv.x, uv.z, u), mix(uv.w, uv.y, v), float(atlas_index));
    }
    return vec3(u, v, float(atlas_index));
}

bool draw_uses_face_buffer(uint draw_flags)
{
    return (draw_flags & kDrawFlagUseFaceBuffer) != 0u;
}

bool draw_uses_descriptor_buffer(uint draw_flags)
{
    return (draw_flags & kDrawFlagUseDescriptorBuffer) != 0u;
}

bool draw_uses_sprite_path(uint draw_flags)
{
    return (draw_flags & kDrawFlagSprite) != 0u;
}

uint get_face_direction(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceDirectionOffset, kFaceDirectionMask);
}

bool get_face_occlusion(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceOcclusionOffset, kFaceOcclusionMask) != 0u;
}

uvec3 get_face_block_position(uvec2 packed_face)
{
    return uvec3(face_extract(packed_face, kFaceXOffset, kFaceXMask),
                 face_extract(packed_face, kFaceYOffset, kFaceYMask),
                 face_extract(packed_face, kFaceZOffset, kFaceZMask));
}

uint get_face_index(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceIndexOffset, kFaceIndexMask);
}

uint get_face_span_u(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceSpanUOffset, kFaceSpanUMask) + 1u;
}

uint get_face_span_v(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceSpanVOffset, kFaceSpanVMask) + 1u;
}

uint get_face_chunk_slot(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceChunkSlotOffset, kFaceChunkSlotMask);
}

uint get_face_water_level(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceWaterLevelOffset, kFaceWaterLevelMask);
}

bool get_face_is_water(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceWaterFlagOffset, kFaceWaterFlagMask) != 0u;
}

uint get_face_water_base_height(uvec2 packed_face)
{
    return face_extract(packed_face, kFaceWaterBaseHeightOffset, kFaceWaterBaseHeightMask);
}

float get_water_surface_height(uint level)
{
    return (8.0 - float(clamp(level, 0u, 7u))) / 9.0;
}

float get_face_water_height(uvec2 packed_face)
{
    if (!get_face_is_water(packed_face))
    {
        return 1.0;
    }
    return get_water_surface_height(get_face_water_level(packed_face));
}

float get_face_water_side_base_height(uvec2 packed_face)
{
    if (!get_face_is_water(packed_face))
    {
        return 0.0;
    }
    return float(clamp(get_face_water_base_height(packed_face), 0u, 7u)) / 9.0;
}

uvec2 get_face_uv_bits(uint corner)
{
    const uvec2 bits[4] = uvec2[4](
        uvec2(0u, 0u), // 0: BL
        uvec2(1u, 0u), // 1: BR
        uvec2(0u, 1u), // 2: TL
        uvec2(1u, 1u)  // 3: TR
    );
    return bits[corner];
}

vec3 get_face_position(uvec2 packed_face, uint vertex_id)
{
    uint direction = get_face_direction(packed_face);
    uvec2 uv_bits = get_face_uv_bits(kQuadIndices[vertex_id % 6u]);
    float u = float(uv_bits.x);
    float v = float(uv_bits.y);
    float su = float(get_face_span_u(packed_face));
    float sv = float(get_face_span_v(packed_face));
    float water_height = get_face_water_height(packed_face);
    bool water_side = get_face_is_water(packed_face) && direction != kDirectionUp && direction != kDirectionDown;
    float side_y = water_side ? mix(get_face_water_side_base_height(packed_face), water_height, v) : v * sv;
    vec3 pos = vec3(get_face_block_position(packed_face));

    if (direction == kDirectionNorth) { pos += vec3(u * su, side_y, 1.0); }
    else if (direction == kDirectionSouth) { pos += vec3((1.0 - u) * su, side_y, 0.0); }
    else if (direction == kDirectionEast) { pos += vec3(1.0, side_y, (1.0 - u) * su); }
    else if (direction == kDirectionWest) { pos += vec3(0.0, side_y, u * su); }
    else if (direction == kDirectionUp) { pos += vec3(u * su, water_height, (1.0 - v) * sv); }
    else if (direction == kDirectionDown) { pos += vec3(u * su, 0.0, v * sv); }

    float inflation = get_face_occlusion(packed_face) ? kFaceInflation : 0.0;
    float offset_u = (u > 0.5) ? inflation : -inflation;
    float offset_v = (v > 0.5) ? inflation : -inflation;
    vec3 expand = vec3(0.0);

    if (direction == kDirectionNorth) { expand = vec3(offset_u, offset_v, 0.0); }
    else if (direction == kDirectionSouth) { expand = vec3(-offset_u, offset_v, 0.0); }
    else if (direction == kDirectionEast) { expand = vec3(0.0, offset_v, -offset_u); }
    else if (direction == kDirectionWest) { expand = vec3(0.0, offset_v, offset_u); }
    else if (direction == kDirectionUp) { expand = vec3(offset_u, 0.0, -offset_v); }
    else if (direction == kDirectionDown) { expand = vec3(offset_u, 0.0, offset_v); }

    return pos + expand;
}

vec3 get_face_texcoord(uvec2 packed_face, uint vertex_id)
{
    uvec2 uv_bits = get_face_uv_bits(kQuadIndices[vertex_id % 6u]);
    return vec3(float(uv_bits.x) * float(get_face_span_u(packed_face)),
                float(uv_bits.y) * float(get_face_span_v(packed_face)),
                float(get_face_index(packed_face)));
}

vec3 get_face_normal(uvec2 packed_face)
{
    return kNormals[get_face_direction(packed_face)];
}

uint pack_face_vertex_voxel(uvec2 packed_face, uint vertex_id)
{
    uint direction = get_face_direction(packed_face);
    uvec2 uv_bits = get_face_uv_bits(kQuadIndices[vertex_id % 6u]);
    uvec3 block = get_face_block_position(packed_face);
    uint voxel = 0u;
    voxel |= uint(get_face_occlusion(packed_face)) << OCCLUSION_OFFSET;
    voxel |= direction << DIRECTION_OFFSET;
    voxel |= block.x << X_OFFSET;
    voxel |= block.y << Y_OFFSET;
    voxel |= block.z << Z_OFFSET;
    voxel |= uv_bits.x << U_OFFSET;
    voxel |= uv_bits.y << V_OFFSET;
    voxel |= get_face_index(packed_face) << INDEX_OFFSET;
    return voxel;
}

vec3 wrap_tiled_texcoord(vec3 texcoord)
{
    return vec3(fract(texcoord.xy), texcoord.z);
}

vec4 get_camera_relative_view_position(mat4 view, vec3 world_position, vec3 camera_position)
{
    mat4 view_rotation = view;
    view_rotation[3][0] = 0.0;
    view_rotation[3][1] = 0.0;
    view_rotation[3][2] = 0.0;
    return view_rotation * vec4(world_position - camera_position, 1.0);
}

vec4 get_camera_relative_view_position_from_chunk(mat4 view, vec3 local_position, ivec2 chunk_position, vec3 camera_position)
{
    mat4 view_rotation = view;
    view_rotation[3][0] = 0.0;
    view_rotation[3][1] = 0.0;
    view_rotation[3][2] = 0.0;
    vec3 relative_position = vec3(float(chunk_position.x) - camera_position.x,
                                  -camera_position.y,
                                  float(chunk_position.y) - camera_position.z);
    relative_position += local_position;
    return view_rotation * vec4(relative_position, 1.0);
}
