#include "world/chunks/build_mesh.h"

#include <vector>

#include "core/check.h"
#include "render/atlas/config.h"
#include "render/world/voxel.h"
#include "render/world/voxel.inc"
#include "world/block/block.h"

namespace {

constexpr float WATER_SURFACE_EPSILON = 0.0f;
constexpr float WATER_FLOW_UV_CENTER = 0.5f;
constexpr float WATER_FLOW_UV_RADIUS = 0.25f;

struct water_flow_t
{
    float x;
    float z;
    Uint32 strength;
};

auto world_chunk_mesh_type_for_block(block_t block) -> mesh_type_t
{
    if (block_is_sprite(block))
    {
        return MESH_TYPE_SPRITE;
    }
    if (block_is_opaque(block))
    {
        return MESH_TYPE_OPAQUE;
    }
    if (block_is_fluid(block))
    {
        return block_is_lava(block) ? MESH_TYPE_LAVA : MESH_TYPE_WATER;
    }
    return MESH_TYPE_TRANSPARENT;
}

auto world_chunk_face_is_visible(block_t block, block_t neighbor) -> bool
{
    if (neighbor == BLOCK_EMPTY)
    {
        return true;
    }
    if (block == BLOCK_GLASS)
    {
        return neighbor != BLOCK_GLASS && !block_has_occlusion(neighbor);
    }
    if (block == BLOCK_CLOUD && neighbor == BLOCK_CLOUD)
    {
        return false;
    }
    if (block == BLOCK_LEAVES && neighbor == BLOCK_LEAVES)
    {
        return true;
    }
    if (block_is_sprite(neighbor))
    {
        return true;
    }
    return block_is_opaque(block) && !block_has_occlusion(neighbor);
}

auto world_chunk_water_height_units(block_t block) -> int
{
    if (!block_is_fluid(block))
    {
        return 0;
    }
    return SDL_clamp(8 - block_get_fluid_level(block), 1, 8);
}

auto world_chunk_water_height(const neighborhood_snapshot_t& snapshot, int bx, int by, int bz) -> float
{
    const block_t block = world_snapshot_neighborhood_block(snapshot, bx, by, bz, 0, 0, 0);
    return static_cast<float>(world_chunk_water_height_units(block)) / 9.0f;
}

auto world_chunk_water_sample_height(const neighborhood_snapshot_t& snapshot,
                                     fluid_kind_t kind,
                                     int bx,
                                     int by,
                                     int bz,
                                     int dx,
                                     int dz) -> float;

auto world_chunk_pack_water_flow(const water_flow_t& flow, int bx, int by, int bz) -> Uint32
{
    const Uint32 seed = static_cast<Uint32>((bx * 17 + by * 31 + bz * 47) & 15);
    return (flow.strength << 4u) | (seed << 8u);
}

auto world_chunk_water_flow(const neighborhood_snapshot_t& snapshot, int bx, int by, int bz) -> water_flow_t
{
    const block_t center = world_snapshot_neighborhood_block(snapshot, bx, by, bz, 0, 0, 0);
    const fluid_kind_t kind = block_get_fluid_kind(center);
    const float center_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, 0, 0);
    const int offsets[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    };
    float flow_x = 0.0f;
    float flow_z = 0.0f;
    for (const auto& offset : offsets)
    {
        const block_t neighbor = world_snapshot_neighborhood_block(snapshot, bx, by, bz, offset[0], 0, offset[1]);
        float pull = 0.0f;
        if (block_get_fluid_kind(neighbor) == kind)
        {
            const float neighbor_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, offset[0], offset[1]);
            pull = center_height - neighbor_height;
        }
        else if (!block_has_occlusion(neighbor))
        {
            const block_t below_neighbor = world_snapshot_neighborhood_block(snapshot, bx, by, bz, offset[0], -1, offset[1]);
            if (block_get_fluid_kind(below_neighbor) == kind)
            {
                const float below_height =
                    world_chunk_water_sample_height(snapshot, kind, bx, by - 1, bz, offset[0], offset[1]);
                pull = center_height - (below_height - 8.0f / 9.0f);
            }
        }
        if (pull != 0.0f)
        {
            flow_x += static_cast<float>(offset[0]) * pull;
            flow_z += static_cast<float>(offset[1]) * pull;
        }
    }
    const float magnitude = SDL_sqrtf(flow_x * flow_x + flow_z * flow_z);
    if (magnitude <= 0.0001f)
    {
        return {};
    }
    const float inv_magnitude = 1.0f / magnitude;
    return {
        .x = flow_x * inv_magnitude,
        .z = flow_z * inv_magnitude,
        .strength = static_cast<Uint32>(SDL_clamp(static_cast<int>(magnitude * 15.0f + 0.5f), 1, 15)),
    };
}

auto world_chunk_water_flow_strength(Uint32 flow) -> Uint32
{
    return (flow >> 4u) & 15u;
}

void world_chunk_water_top_texcoords(const water_flow_t& flow, float texcoords[4][2])
{
    if (flow.strength == 0u)
    {
        texcoords[0][0] = 0.0f;
        texcoords[0][1] = 0.0f;
        texcoords[1][0] = 1.0f;
        texcoords[1][1] = 0.0f;
        texcoords[2][0] = 1.0f;
        texcoords[2][1] = 1.0f;
        texcoords[3][0] = 0.0f;
        texcoords[3][1] = 1.0f;
        return;
    }

    const float angle = SDL_atan2f(-flow.z, flow.x) - SDL_PI_F * 0.5f;
    const float s = SDL_sinf(angle) * WATER_FLOW_UV_RADIUS;
    const float c = SDL_cosf(angle) * WATER_FLOW_UV_RADIUS;

    texcoords[0][0] = WATER_FLOW_UV_CENTER + (-c - s);
    texcoords[0][1] = WATER_FLOW_UV_CENTER + (-c + s);
    texcoords[1][0] = WATER_FLOW_UV_CENTER + (c - s);
    texcoords[1][1] = WATER_FLOW_UV_CENTER + (-c - s);
    texcoords[2][0] = WATER_FLOW_UV_CENTER + (c + s);
    texcoords[2][1] = WATER_FLOW_UV_CENTER + (c - s);
    texcoords[3][0] = WATER_FLOW_UV_CENTER + (-c + s);
    texcoords[3][1] = WATER_FLOW_UV_CENTER + (c + s);
}

auto world_chunk_water_atlas_layer(block_t block, direction_t direction, Uint32 flow) -> int
{
    const fluid_kind_t kind = block_get_fluid_kind(block);
    const int still_layer = kind == FLUID_LAVA ? MAIN_RENDER_ATLAS_LAVA_STILL_LAYER
                                               : MAIN_RENDER_ATLAS_WATER_STILL_LAYER;
    const int flow_layer = kind == FLUID_LAVA ? MAIN_RENDER_ATLAS_LAVA_FLOW_LAYER
                                              : MAIN_RENDER_ATLAS_WATER_FLOW_LAYER;
    switch (direction)
    {
        case DIRECTION_NORTH:
        case DIRECTION_SOUTH:
        case DIRECTION_EAST:
        case DIRECTION_WEST:
            return flow_layer;
        case DIRECTION_UP:
            return world_chunk_water_flow_strength(flow) > 0u ? flow_layer : still_layer;
        case DIRECTION_DOWN:
            return still_layer;
        default:
            return still_layer;
    }
}

auto world_chunk_water_sample_height(const neighborhood_snapshot_t& snapshot,
                                     fluid_kind_t kind,
                                     int bx,
                                     int by,
                                     int bz,
                                     int dx,
                                     int dz) -> float
{
    const block_t block = world_snapshot_neighborhood_block(snapshot, bx, by, bz, dx, 0, dz);
    if (block_get_fluid_kind(block) != kind)
    {
        return block_is_solid(block) ? -1.0f : 0.0f;
    }
    const block_t above = world_snapshot_neighborhood_block(snapshot, bx, by, bz, dx, 1, dz);
    return block_get_fluid_kind(above) == kind ? 1.0f : static_cast<float>(world_chunk_water_height_units(block)) / 9.0f;
}

auto world_chunk_water_corner_height(const neighborhood_snapshot_t& snapshot,
                                     fluid_kind_t kind,
                                     int bx,
                                     int by,
                                     int bz,
                                     int dx,
                                     int dz) -> float
{
    const float self_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, 0, 0);
    const float x_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, dx, 0);
    const float z_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, 0, dz);
    if (x_height >= 1.0f || z_height >= 1.0f)
    {
        return 1.0f;
    }

    float total = 0.0f;
    float count = 0.0f;
    const auto add_weighted_height = [&](float height)
    {
        if (height >= 0.8f)
        {
            total += height * 10.0f;
            count += 10.0f;
        }
        else if (height >= 0.0f)
        {
            total += height;
            count += 1.0f;
        }
    };

    if (x_height > 0.0f || z_height > 0.0f)
    {
        const float diagonal_height = world_chunk_water_sample_height(snapshot, kind, bx, by, bz, dx, dz);
        if (diagonal_height >= 1.0f)
        {
            return 1.0f;
        }
        add_weighted_height(diagonal_height);
    }
    add_weighted_height(self_height);
    add_weighted_height(x_height);
    add_weighted_height(z_height);
    if (count <= 0.0f)
    {
        return world_chunk_water_height(snapshot, bx, by, bz);
    }
    return total / count;
}

template <typename AppendWater>
void world_chunk_append_water_vertex(float x,
                                     float y,
                                     float z,
                                     float u,
                                     float v,
                                     float fill,
                                     Uint32 flow,
                                     direction_t direction,
                                     int atlas_layer,
                                     int level,
                                     AppendWater&& append_water)
{
    water_vertex_t vertex = {};
    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.position[2] = z;
    vertex.position[3] = 1.0f;
    vertex.texcoord[0] = u;
    vertex.texcoord[1] = v;
    vertex.texcoord[2] = static_cast<float>(atlas_layer);
    vertex.texcoord[3] = SDL_clamp(fill, 0.0f, 1.0f);
    vertex.data[0] = static_cast<Uint32>(direction);
    vertex.data[1] = static_cast<Uint32>(level);
    vertex.data[2] = static_cast<Uint32>(atlas_layer);
    vertex.data[3] = flow;
    append_water(vertex);
}

template <typename AppendWater>
void world_chunk_append_water_indexed_quad(block_t block,
                                           int by,
                                           Uint32 flow,
                                           const float positions[4][3],
                                           const float texcoords[4][2],
                                           direction_t direction,
                                           const int indices[6],
                                           AppendWater&& append_water)
{
    const int level = SDL_clamp(block_get_fluid_level(block), 0, 7);
    const int atlas_layer = world_chunk_water_atlas_layer(block, direction, flow);
    for (int i = 0; i < 6; ++i)
    {
        const int vertex = indices[i];
        world_chunk_append_water_vertex(positions[vertex][0],
                                        positions[vertex][1],
                                        positions[vertex][2],
                                        texcoords[vertex][0],
                                        texcoords[vertex][1],
                                        positions[vertex][1] - static_cast<float>(by),
                                        flow,
                                        direction,
                                        atlas_layer,
                                        level,
                                        append_water);
    }
}

template <typename AppendWater>
void world_chunk_append_water_quad(block_t block,
                                   int by,
                                   Uint32 flow,
                                   const float positions[4][3],
                                   const float texcoords[4][2],
                                   direction_t direction,
                                   AppendWater&& append_water)
{
    static constexpr int INDICES[6] = {0, 1, 2, 0, 2, 3};
    world_chunk_append_water_indexed_quad(block, by, flow, positions, texcoords, direction, INDICES, append_water);
}

template <typename AppendWater>
void world_chunk_append_water_top_quad(block_t block,
                                       int by,
                                       Uint32 flow,
                                       const float positions[4][3],
                                       const float texcoords[4][2],
                                       AppendWater&& append_water)
{
    static constexpr int DIAGONAL_02[6] = {0, 1, 2, 0, 2, 3};
    static constexpr int DIAGONAL_13[6] = {0, 1, 3, 1, 2, 3};
    const float diagonal_02_error = SDL_fabsf(positions[0][1] - positions[2][1]);
    const float diagonal_13_error = SDL_fabsf(positions[1][1] - positions[3][1]);
    const int* indices = diagonal_13_error < diagonal_02_error ? DIAGONAL_13 : DIAGONAL_02;
    world_chunk_append_water_indexed_quad(block, by, flow, positions, texcoords, DIRECTION_UP, indices, append_water);
}

template <typename AppendWater>
void world_chunk_append_water_triangle(block_t block,
                                       int by,
                                       Uint32 flow,
                                       const float positions[3][3],
                                       const float texcoords[3][2],
                                       direction_t direction,
                                       AppendWater&& append_water)
{
    const int level = SDL_clamp(block_get_fluid_level(block), 0, 7);
    const int atlas_layer = world_chunk_water_atlas_layer(block, direction, flow);
    for (int vertex = 0; vertex < 3; ++vertex)
    {
        world_chunk_append_water_vertex(positions[vertex][0],
                                        positions[vertex][1],
                                        positions[vertex][2],
                                        texcoords[vertex][0],
                                        texcoords[vertex][1],
                                        positions[vertex][1] - static_cast<float>(by),
                                        flow,
                                        direction,
                                        atlas_layer,
                                        level,
                                        append_water);
    }
}

template <typename AppendWater>
void world_chunk_append_water_mesh(const neighborhood_snapshot_t& snapshot,
                                   block_t block,
                                   int bx,
                                   int by,
                                   int bz,
                                   AppendWater&& append_water)
{
    const fluid_kind_t kind = block_get_fluid_kind(block);
    float top_nw = world_chunk_water_corner_height(snapshot, kind, bx, by, bz, -1, 1);
    float top_ne = world_chunk_water_corner_height(snapshot, kind, bx, by, bz, 1, 1);
    float top_se = world_chunk_water_corner_height(snapshot, kind, bx, by, bz, 1, -1);
    float top_sw = world_chunk_water_corner_height(snapshot, kind, bx, by, bz, -1, -1);
    const water_flow_t flow = world_chunk_water_flow(snapshot, bx, by, bz);
    const Uint32 packed_flow = world_chunk_pack_water_flow(flow, bx, by, bz);
    const block_t above = world_snapshot_neighborhood_block(snapshot, bx, by, bz, 0, 1, 0);
    const bool has_fluid_above = block_get_fluid_kind(above) == kind;
    const bool render_top = !has_fluid_above && !block_is_solid(above);
    if (render_top)
    {
        top_nw = SDL_max(top_nw - WATER_SURFACE_EPSILON, 0.0f);
        top_ne = SDL_max(top_ne - WATER_SURFACE_EPSILON, 0.0f);
        top_se = SDL_max(top_se - WATER_SURFACE_EPSILON, 0.0f);
        top_sw = SDL_max(top_sw - WATER_SURFACE_EPSILON, 0.0f);
    }
    if (render_top)
    {
        const float corners[4][3] = {
            {static_cast<float>(bx),     static_cast<float>(by) + top_nw, static_cast<float>(bz + 1)},
            {static_cast<float>(bx + 1), static_cast<float>(by) + top_ne, static_cast<float>(bz + 1)},
            {static_cast<float>(bx + 1), static_cast<float>(by) + top_se, static_cast<float>(bz)},
            {static_cast<float>(bx),     static_cast<float>(by) + top_sw, static_cast<float>(bz)},
        };
        float corner_texcoords[4][2] = {};
        world_chunk_water_top_texcoords(flow, corner_texcoords);
        world_chunk_append_water_top_quad(block, by, packed_flow, corners, corner_texcoords, append_water);
    }

    struct water_side
    {
        direction_t direction;
        int dx;
        int dz;
        float a[3];
        float b[3];
        float top_a;
        float top_b;
    };
    const float side_top_nw = has_fluid_above ? 1.0f : top_nw;
    const float side_top_ne = has_fluid_above ? 1.0f : top_ne;
    const float side_top_se = has_fluid_above ? 1.0f : top_se;
    const float side_top_sw = has_fluid_above ? 1.0f : top_sw;
    const water_side sides[] = {
        {DIRECTION_NORTH, 0, 1, {static_cast<float>(bx),     0.0f, static_cast<float>(bz + 1)},
                                {static_cast<float>(bx + 1), 0.0f, static_cast<float>(bz + 1)}, side_top_nw, side_top_ne},
        {DIRECTION_SOUTH, 0, -1, {static_cast<float>(bx + 1), 0.0f, static_cast<float>(bz)},
                                 {static_cast<float>(bx),     0.0f, static_cast<float>(bz)},     side_top_se, side_top_sw},
        {DIRECTION_EAST, 1, 0, {static_cast<float>(bx + 1), 0.0f, static_cast<float>(bz + 1)},
                               {static_cast<float>(bx + 1), 0.0f, static_cast<float>(bz)},     side_top_ne, side_top_se},
        {DIRECTION_WEST, -1, 0, {static_cast<float>(bx), 0.0f, static_cast<float>(bz)},
                                {static_cast<float>(bx), 0.0f, static_cast<float>(bz + 1)}, side_top_sw, side_top_nw},
    };

    for (const water_side& side : sides)
    {
        const block_t neighbor = world_snapshot_neighborhood_block(snapshot, bx, by, bz, side.dx, 0, side.dz);
        const bool same_fluid_neighbor = block_get_fluid_kind(neighbor) == kind;
        if (same_fluid_neighbor || block_is_solid(neighbor))
        {
            continue;
        }

        float base_a = 0.0f;
        float base_b = 0.0f;
        float top_a = side.top_a;
        float top_b = side.top_b;
        top_a = SDL_max(top_a, base_a);
        top_b = SDL_max(top_b, base_b);
        if (base_a >= top_a - WATER_SURFACE_EPSILON && base_b >= top_b - WATER_SURFACE_EPSILON)
        {
            continue;
        }
        top_a = SDL_clamp(top_a, 0.0f, 1.0f);
        top_b = SDL_clamp(top_b, 0.0f, 1.0f);
        const float positions[4][3] = {
            {side.a[0], static_cast<float>(by) + top_a, side.a[2]},
            {side.b[0], static_cast<float>(by) + top_b, side.b[2]},
            {side.b[0], static_cast<float>(by) + base_b, side.b[2]},
            {side.a[0], static_cast<float>(by) + base_a, side.a[2]},
        };
        const float texcoords[4][2] = {
            {0.0f, (1.0f - top_a) * 0.5f},
            {0.5f, (1.0f - top_b) * 0.5f},
            {0.5f, 0.5f},
            {0.0f, 0.5f},
        };
        world_chunk_append_water_quad(block, by, packed_flow, positions, texcoords, side.direction, append_water);
    }

    const block_t below = world_snapshot_neighborhood_block(snapshot, bx, by, bz, 0, -1, 0);
    if (block_get_fluid_kind(below) != kind && !block_is_solid(below))
    {
        const float y = static_cast<float>(by);
        const float positions[4][3] = {
            {static_cast<float>(bx),     y, static_cast<float>(bz)},
            {static_cast<float>(bx + 1), y, static_cast<float>(bz)},
            {static_cast<float>(bx + 1), y, static_cast<float>(bz + 1)},
            {static_cast<float>(bx),     y, static_cast<float>(bz + 1)},
        };
        const float texcoords[4][2] = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f},
        };
        world_chunk_append_water_quad(block, by, packed_flow, positions, texcoords, DIRECTION_DOWN, append_water);
    }
}

template <typename AppendSprite>
void world_chunk_append_sprite_mesh(block_t block, int bx, int by, int bz, AppendSprite&& append_sprite)
{
    const int face_count = block_requires_solid_base(block) ? DIRECTION_COUNT : 4;
    for (int direction = 0; direction < face_count; ++direction)
    for (int vertex = 0; vertex < 4; ++vertex)
    {
        const voxel_t packed = voxel_pack_sprite(block, bx, by, bz, static_cast<direction_t>(direction), vertex);
        append_sprite(packed);
    }
}

[[maybe_unused]] auto world_chunk_pack_legacy_cube_vertex(block_t block,
                                                          int x,
                                                          int y,
                                                          int z,
                                                          direction_t direction,
                                                          int u,
                                                          int v) -> voxel_t
{
    CHECK(direction >= DIRECTION_NORTH);
    CHECK(direction < DIRECTION_COUNT);
    CHECK(x >= 0 && x <= X_MASK);
    CHECK(y >= 0 && y <= Y_MASK);
    CHECK(z >= 0 && z <= Z_MASK);
    CHECK(u >= 0 && u <= U_MASK);
    CHECK(v >= 0 && v <= V_MASK);

    voxel_t voxel = 0;
    voxel |= static_cast<voxel_t>(block_has_occlusion(block)) << OCCLUSION_OFFSET;
    voxel |= static_cast<voxel_t>(direction) << DIRECTION_OFFSET;
    voxel |= static_cast<voxel_t>(x) << X_OFFSET;
    voxel |= static_cast<voxel_t>(y) << Y_OFFSET;
    voxel |= static_cast<voxel_t>(z) << Z_OFFSET;
    voxel |= static_cast<voxel_t>(u) << U_OFFSET;
    voxel |= static_cast<voxel_t>(v) << V_OFFSET;
    voxel |= static_cast<voxel_t>(block_get_index(block, direction)) << INDEX_OFFSET;
    return voxel;
}

template <typename AppendLegacyCube>
void world_chunk_append_legacy_face_vertices(block_t block,
                                             int x,
                                             int y,
                                             int z,
                                             direction_t direction,
                                             AppendLegacyCube&& append_legacy_cube)
{
    struct uv_bits
    {
        Uint8 u;
        Uint8 v;
    };

    static constexpr uv_bits PATTERN_A[4] = {
        {1u, 1u},
        {0u, 1u},
        {1u, 0u},
        {0u, 0u},
    };
    static constexpr uv_bits PATTERN_B[4] = {
        {1u, 1u},
        {1u, 0u},
        {0u, 1u},
        {0u, 0u},
    };

    const uv_bits* pattern = PATTERN_A;
    if (direction == DIRECTION_SOUTH || direction == DIRECTION_EAST || direction == DIRECTION_UP)
    {
        pattern = PATTERN_B;
    }

    for (int vertex = 0; vertex < 4; ++vertex)
    {
        const voxel_t packed =
            world_chunk_pack_legacy_cube_vertex(block, x, y, z, direction, pattern[vertex].u, pattern[vertex].v);
        append_legacy_cube(packed);
    }
}

template <typename AppendLegacyCube>
void world_chunk_append_legacy_face_mesh(const packed_face_desc_t& face, block_t block, AppendLegacyCube&& append_legacy_cube)
{
    for (int u = 0; u < face.u_extent; ++u)
    for (int v = 0; v < face.v_extent; ++v)
    {
        int bx = face.x;
        int by = face.y;
        int bz = face.z;

        switch (static_cast<direction_t>(face.direction))
        {
            case DIRECTION_NORTH:
            case DIRECTION_SOUTH:
                bx += u;
                by += v;
                break;
            case DIRECTION_EAST:
            case DIRECTION_WEST:
                bz += u;
                by += v;
                break;
            case DIRECTION_UP:
            case DIRECTION_DOWN:
                bx += u;
                bz += v;
                break;
            default:
                CHECK(false);
                break;
        }

        world_chunk_append_legacy_face_vertices(block, bx, by, bz, static_cast<direction_t>(face.direction),
                                                append_legacy_cube);
    }
}

template <typename AppendCube, typename AppendWater, typename AppendSprite>
void world_chunk_build_meshes_impl(const neighborhood_snapshot_t& snapshot,
                                   AppendCube&& append_cube,
                                   AppendWater&& append_water,
                                   AppendSprite&& append_sprite)
{
    for (int bx = 0; bx < CHUNK_WIDTH; ++bx)
    for (int by = 0; by < CHUNK_HEIGHT; ++by)
    for (int bz = 0; bz < CHUNK_WIDTH; ++bz)
    {
        const block_t block = world_snapshot_local_block(snapshot, 1, 1, bx, by, bz);
        if (block == BLOCK_EMPTY)
        {
            continue;
        }
        if (block_is_sprite(block))
        {
            world_chunk_append_sprite_mesh(block, bx, by, bz, append_sprite);
            continue;
        }
        if (block_is_fluid(block))
        {
            const mesh_type_t mesh_type = world_chunk_mesh_type_for_block(block);
            world_chunk_append_water_mesh(snapshot,
                                          block,
                                          bx,
                                          by,
                                          bz,
                                          [&](const water_vertex_t& water_vertex)
                                          {
                                              append_water(mesh_type, water_vertex);
                                          });
            continue;
        }
        if (block == BLOCK_CLOUD)
        {
            continue;
        }

        const mesh_type_t mesh_type = world_chunk_mesh_type_for_block(block);
        for (int direction = 0; direction < DIRECTION_COUNT; ++direction)
        {
            const direction_t typed_direction = static_cast<direction_t>(direction);
            const int dx = DIRECTIONS[direction][0];
            const int dy = DIRECTIONS[direction][1];
            const int dz = DIRECTIONS[direction][2];
            const block_t neighbor = world_snapshot_neighborhood_block(snapshot, bx, by, bz, dx, dy, dz);
            if (world_chunk_face_is_visible(block, neighbor))
            {
                append_cube(block,
                            mesh_type,
                            voxel_pack_cube_face(block, bx, by, bz, typed_direction, 1, 1));
            }
        }
    }
}

} // namespace

void world_chunk_build_meshes(const neighborhood_snapshot_t& snapshot, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    meshes[MESH_TYPE_OPAQUE].stride = sizeof(packed_face_t);
    meshes[MESH_TYPE_OPAQUE].payload = CPU_BUFFER_PAYLOAD_PACKED_FACE;
    meshes[MESH_TYPE_TRANSPARENT].stride = sizeof(packed_face_t);
    meshes[MESH_TYPE_TRANSPARENT].payload = CPU_BUFFER_PAYLOAD_PACKED_FACE;
    meshes[MESH_TYPE_WATER].stride = sizeof(water_vertex_t);
    meshes[MESH_TYPE_WATER].payload = CPU_BUFFER_PAYLOAD_WATER_VERTEX;
    meshes[MESH_TYPE_LAVA].stride = sizeof(water_vertex_t);
    meshes[MESH_TYPE_LAVA].payload = CPU_BUFFER_PAYLOAD_WATER_VERTEX;
    meshes[MESH_TYPE_SPRITE].stride = sizeof(sprite_voxel_t);
    meshes[MESH_TYPE_SPRITE].payload = CPU_BUFFER_PAYLOAD_SPRITE_VOXEL;

    world_chunk_build_meshes_impl(
        snapshot,
        [&](block_t, mesh_type_t mesh_type, packed_face_t face)
        {
            voxel_t value = face;
            cpu_buffer_append(&meshes[mesh_type], &value);
        },
        [&](mesh_type_t mesh_type, const water_vertex_t& water_vertex)
        {
            water_vertex_t value = water_vertex;
            cpu_buffer_append(&meshes[mesh_type], &value);
        },
        [&](voxel_t sprite)
        {
            sprite_voxel_t value = static_cast<sprite_voxel_t>(sprite);
            cpu_buffer_append(&meshes[MESH_TYPE_SPRITE], &value);
        });
}

void world_chunk_build_mesh_vectors(const neighborhood_snapshot_t& snapshot, std::vector<voxel_t> meshes[MESH_TYPE_COUNT])
{
    world_chunk_build_meshes_impl(
        snapshot,
        [&](block_t, mesh_type_t mesh_type, packed_face_t face)
        {
            meshes[mesh_type].push_back(face);
        },
        [&](mesh_type_t, const water_vertex_t&)
        {
        },
        [&](voxel_t sprite)
        {
            meshes[MESH_TYPE_SPRITE].push_back(sprite);
        });
}
