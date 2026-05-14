#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "world/direction.h"

typedef Uint64 voxel_t;
typedef Uint64 packed_face_t;
typedef Uint32 sprite_voxel_t;

typedef struct water_vertex
{
    float position[4];
    float texcoord[4];
    Uint32 data[4];
}
water_vertex_t;

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i);

typedef struct packed_face_desc
{
    Uint8 x;
    Uint8 y;
    Uint8 z;
    Uint8 u_extent;
    Uint8 v_extent;
    Uint8 atlas_index;
    Uint8 direction;
    bool has_occlusion;
    Uint16 chunk_slot;
}
packed_face_desc_t;

inline constexpr Uint16 PACKED_FACE_CHUNK_SLOT_UNSET = 0x1FFFu;

// Cube faces use per-direction UV axes:
// - north/south: U = X, V = Y
// - east/west: U = Z, V = Y
// - up/down: U = X, V = Z
packed_face_t voxel_pack_cube_face(block_t block,
                                   int x,
                                   int y,
                                   int z,
                                   direction_t direction,
                                   int u_extent,
                                   int v_extent);
packed_face_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction);
packed_face_t voxel_face_set_chunk_slot(packed_face_t face, Uint32 chunk_slot);
Uint32 voxel_face_get_chunk_slot(packed_face_t face);
packed_face_desc_t voxel_unpack_cube_face(packed_face_t face);
