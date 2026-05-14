#include <SDL3/SDL.h>

#include "core/check.h"
#include "render/world/voxel.h"
#include "render/world/voxel.inc"
#include "world/block/block.h"
#include "world/direction.h"
#include "world/runtime/world.h"

namespace {

constexpr Uint32 FACE_X_BITS = 5;
constexpr Uint32 FACE_Y_BITS = 8;
constexpr Uint32 FACE_Z_BITS = 5;
constexpr Uint32 FACE_DIRECTION_BITS = 3;
constexpr Uint32 FACE_U_EXTENT_BITS = 8;
constexpr Uint32 FACE_V_EXTENT_BITS = 8;
constexpr Uint32 FACE_INDEX_BITS = 6;
constexpr Uint32 FACE_OCCLUSION_BITS = 1;
constexpr Uint32 FACE_CHUNK_SLOT_BITS = 13;
constexpr Uint32 FACE_WATER_LEVEL_BITS = 3;
constexpr Uint32 FACE_WATER_FLAG_BITS = 1;
constexpr Uint32 FACE_WATER_BASE_HEIGHT_BITS = 3;

constexpr Uint32 FACE_X_OFFSET = 0;
constexpr Uint32 FACE_Y_OFFSET = FACE_X_OFFSET + FACE_X_BITS;
constexpr Uint32 FACE_Z_OFFSET = FACE_Y_OFFSET + FACE_Y_BITS;
constexpr Uint32 FACE_DIRECTION_OFFSET = FACE_Z_OFFSET + FACE_Z_BITS;
constexpr Uint32 FACE_U_EXTENT_OFFSET = FACE_DIRECTION_OFFSET + FACE_DIRECTION_BITS;
constexpr Uint32 FACE_V_EXTENT_OFFSET = FACE_U_EXTENT_OFFSET + FACE_U_EXTENT_BITS;
constexpr Uint32 FACE_INDEX_OFFSET = FACE_V_EXTENT_OFFSET + FACE_V_EXTENT_BITS;
constexpr Uint32 FACE_OCCLUSION_OFFSET = FACE_INDEX_OFFSET + FACE_INDEX_BITS;
constexpr Uint32 FACE_CHUNK_SLOT_OFFSET = FACE_OCCLUSION_OFFSET + FACE_OCCLUSION_BITS;
constexpr Uint32 FACE_WATER_LEVEL_OFFSET = FACE_CHUNK_SLOT_OFFSET + FACE_CHUNK_SLOT_BITS;
constexpr Uint32 FACE_WATER_FLAG_OFFSET = FACE_WATER_LEVEL_OFFSET + FACE_WATER_LEVEL_BITS;
constexpr Uint32 FACE_WATER_BASE_HEIGHT_OFFSET = FACE_WATER_FLAG_OFFSET + FACE_WATER_FLAG_BITS;

constexpr Uint64 face_mask(Uint32 bits)
{
    return (Uint64{1} << bits) - 1u;
}

constexpr Uint64 FACE_X_MASK = face_mask(FACE_X_BITS);
constexpr Uint64 FACE_Y_MASK = face_mask(FACE_Y_BITS);
constexpr Uint64 FACE_Z_MASK = face_mask(FACE_Z_BITS);
constexpr Uint64 FACE_DIRECTION_MASK = face_mask(FACE_DIRECTION_BITS);
constexpr Uint64 FACE_U_EXTENT_MASK = face_mask(FACE_U_EXTENT_BITS);
constexpr Uint64 FACE_V_EXTENT_MASK = face_mask(FACE_V_EXTENT_BITS);
constexpr Uint64 FACE_INDEX_MASK = face_mask(FACE_INDEX_BITS);
constexpr Uint64 FACE_OCCLUSION_MASK = face_mask(FACE_OCCLUSION_BITS);
constexpr Uint64 FACE_CHUNK_SLOT_MASK = face_mask(FACE_CHUNK_SLOT_BITS);
constexpr Uint64 FACE_WATER_LEVEL_MASK = face_mask(FACE_WATER_LEVEL_BITS);
constexpr Uint64 FACE_WATER_FLAG_MASK = face_mask(FACE_WATER_FLAG_BITS);
constexpr Uint64 FACE_WATER_BASE_HEIGHT_MASK = face_mask(FACE_WATER_BASE_HEIGHT_BITS);

SDL_COMPILE_TIME_ASSERT("", FACE_WATER_BASE_HEIGHT_OFFSET + FACE_WATER_BASE_HEIGHT_BITS <= 64);

auto pack_sprite(block_t block, int x, int y, int z, int u, int v, int sprite_direction) -> sprite_voxel_t
{
    SDL_COMPILE_TIME_ASSERT("", X_OFFSET + X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Y_OFFSET + Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Z_OFFSET + Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", U_OFFSET + U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", V_OFFSET + V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", INDEX_OFFSET + INDEX_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", DIRECTION_OFFSET + DIRECTION_BITS <= 32);
    const int index = block_get_index(block, DIRECTION_NORTH);
    CHECK(x <= X_MASK);
    CHECK(y <= Y_MASK);
    CHECK(z <= Z_MASK);
    CHECK(u <= U_MASK);
    CHECK(v <= V_MASK);
    CHECK(index <= INDEX_MASK);
    CHECK(sprite_direction <= DIRECTION_MASK);
    sprite_voxel_t voxel = 0;
    voxel |= static_cast<sprite_voxel_t>(block_has_occlusion(block)) << OCCLUSION_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(sprite_direction) << DIRECTION_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(x) << X_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(y) << Y_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(z) << Z_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(u) << U_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(v) << V_OFFSET;
    voxel |= static_cast<sprite_voxel_t>(index) << INDEX_OFFSET;
    return voxel;
}

void validate_cube_face_inputs(block_t block, int x, int y, int z, direction_t direction, int u_extent, int v_extent)
{
    CHECK(block > BLOCK_EMPTY);
    CHECK(block < BLOCK_COUNT);
    CHECK(!block_is_sprite(block));
    CHECK(direction >= DIRECTION_NORTH);
    CHECK(direction < DIRECTION_COUNT);
    CHECK(x >= 0);
    CHECK(x < CHUNK_WIDTH);
    CHECK(y >= 0);
    CHECK(y < CHUNK_HEIGHT);
    CHECK(z >= 0);
    CHECK(z < CHUNK_WIDTH);
    CHECK(u_extent > 0);
    CHECK(v_extent > 0);
    switch (direction)
    {
        case DIRECTION_NORTH:
        case DIRECTION_SOUTH:
            CHECK(x + u_extent <= CHUNK_WIDTH);
            CHECK(y + v_extent <= CHUNK_HEIGHT);
            break;
        case DIRECTION_EAST:
        case DIRECTION_WEST:
            CHECK(z + u_extent <= CHUNK_WIDTH);
            CHECK(y + v_extent <= CHUNK_HEIGHT);
            break;
        case DIRECTION_UP:
        case DIRECTION_DOWN:
            CHECK(x + u_extent <= CHUNK_WIDTH);
            CHECK(z + v_extent <= CHUNK_WIDTH);
            break;
        default:
            CHECK(false);
            break;
    }
}

auto pack_face_field(packed_face_t face, Uint64 value, Uint32 offset, Uint64 mask) -> packed_face_t
{
    CHECK(value <= mask);
    face |= (value & mask) << offset;
    return face;
}

} // namespace

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i)
{
    CHECK(block > BLOCK_EMPTY);
    CHECK(block < BLOCK_COUNT);
    CHECK(direction < DIRECTION_COUNT);
    CHECK(i < 4);
    static const int TEXCOORDS[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
        {{0, 1}, {0, 0}, {1, 1}, {1, 0}},
    };
    const int d = TEXCOORDS[direction][i][0];
    const int e = TEXCOORDS[direction][i][1];
    return static_cast<voxel_t>(pack_sprite(block, x, y, z, d, e, DIRECTION_COUNT + direction));
}

packed_face_t voxel_pack_cube_face(block_t block,
                                   int x,
                                   int y,
                                   int z,
                                   direction_t direction,
                                   int u_extent,
                                   int v_extent,
                                   int water_base_height)
{
    validate_cube_face_inputs(block, x, y, z, direction, u_extent, v_extent);
    const int atlas_index = block_get_index(block, direction);
    CHECK(atlas_index <= static_cast<int>(FACE_INDEX_MASK));
    CHECK(water_base_height >= 0);
    CHECK(water_base_height <= static_cast<int>(FACE_WATER_BASE_HEIGHT_MASK));

    packed_face_t face = 0;
    face = pack_face_field(face, static_cast<Uint64>(x), FACE_X_OFFSET, FACE_X_MASK);
    face = pack_face_field(face, static_cast<Uint64>(y), FACE_Y_OFFSET, FACE_Y_MASK);
    face = pack_face_field(face, static_cast<Uint64>(z), FACE_Z_OFFSET, FACE_Z_MASK);
    face = pack_face_field(face, static_cast<Uint64>(direction), FACE_DIRECTION_OFFSET, FACE_DIRECTION_MASK);
    face = pack_face_field(face, static_cast<Uint64>(u_extent - 1), FACE_U_EXTENT_OFFSET, FACE_U_EXTENT_MASK);
    face = pack_face_field(face, static_cast<Uint64>(v_extent - 1), FACE_V_EXTENT_OFFSET, FACE_V_EXTENT_MASK);
    face = pack_face_field(face, static_cast<Uint64>(atlas_index), FACE_INDEX_OFFSET, FACE_INDEX_MASK);
    face = pack_face_field(face, static_cast<Uint64>(block_has_occlusion(block)), FACE_OCCLUSION_OFFSET, FACE_OCCLUSION_MASK);
    face = pack_face_field(face, PACKED_FACE_CHUNK_SLOT_UNSET, FACE_CHUNK_SLOT_OFFSET, FACE_CHUNK_SLOT_MASK);
    face = pack_face_field(face, static_cast<Uint64>(SDL_max(block_get_water_level(block), 0)), FACE_WATER_LEVEL_OFFSET,
                           FACE_WATER_LEVEL_MASK);
    face = pack_face_field(face, static_cast<Uint64>(block_is_water(block)), FACE_WATER_FLAG_OFFSET, FACE_WATER_FLAG_MASK);
    face = pack_face_field(face, static_cast<Uint64>(block_is_water(block) ? water_base_height : 0),
                           FACE_WATER_BASE_HEIGHT_OFFSET, FACE_WATER_BASE_HEIGHT_MASK);
    return face;
}

packed_face_t voxel_pack_cube_face(block_t block, int x, int y, int z, direction_t direction, int u_extent, int v_extent)
{
    return voxel_pack_cube_face(block, x, y, z, direction, u_extent, v_extent, 0);
}

packed_face_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction)
{
    return voxel_pack_cube_face(block, x, y, z, direction, 1, 1);
}

packed_face_t voxel_face_set_chunk_slot(packed_face_t face, Uint32 chunk_slot)
{
    CHECK(chunk_slot < PACKED_FACE_CHUNK_SLOT_UNSET);
    const packed_face_t cleared = face & ~(FACE_CHUNK_SLOT_MASK << FACE_CHUNK_SLOT_OFFSET);
    return cleared | (static_cast<packed_face_t>(chunk_slot) << FACE_CHUNK_SLOT_OFFSET);
}

Uint32 voxel_face_get_chunk_slot(packed_face_t face)
{
    return static_cast<Uint32>((face >> FACE_CHUNK_SLOT_OFFSET) & FACE_CHUNK_SLOT_MASK);
}

packed_face_desc_t voxel_unpack_cube_face(packed_face_t face)
{
    packed_face_desc_t desc = {};
    desc.x = static_cast<Uint8>((face >> FACE_X_OFFSET) & FACE_X_MASK);
    desc.y = static_cast<Uint8>((face >> FACE_Y_OFFSET) & FACE_Y_MASK);
    desc.z = static_cast<Uint8>((face >> FACE_Z_OFFSET) & FACE_Z_MASK);
    desc.direction = static_cast<Uint8>((face >> FACE_DIRECTION_OFFSET) & FACE_DIRECTION_MASK);
    desc.u_extent = static_cast<Uint8>(((face >> FACE_U_EXTENT_OFFSET) & FACE_U_EXTENT_MASK) + 1u);
    desc.v_extent = static_cast<Uint8>(((face >> FACE_V_EXTENT_OFFSET) & FACE_V_EXTENT_MASK) + 1u);
    desc.atlas_index = static_cast<Uint8>((face >> FACE_INDEX_OFFSET) & FACE_INDEX_MASK);
    desc.has_occlusion = ((face >> FACE_OCCLUSION_OFFSET) & FACE_OCCLUSION_MASK) != 0;
    desc.chunk_slot = static_cast<Uint16>((face >> FACE_CHUNK_SLOT_OFFSET) & FACE_CHUNK_SLOT_MASK);
    return desc;
}
