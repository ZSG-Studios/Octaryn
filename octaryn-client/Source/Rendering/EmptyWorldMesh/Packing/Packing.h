#pragma once

#include <cstdint>

constexpr int32_t kEmptyWorldChunkSize = 32;
constexpr int32_t kEmptyWorldMinY = -256;
constexpr int32_t kEmptyWorldMaxYExclusive = 256;
constexpr int32_t kEmptyWorldMinChunkY = -8;
constexpr int32_t kEmptyWorldChunkY = -1;
constexpr int32_t kEmptyWorldLocalY = 31;

uint64_t pack_empty_world_block_face(uint32_t x, uint32_t y, uint32_t z,
                                     uint32_t direction, uint32_t span_u,
                                     uint32_t span_v);
uint64_t pack_empty_world_block_face_with_layer(
    uint32_t x, uint32_t y, uint32_t z, uint32_t direction, uint32_t span_u,
    uint32_t span_v, uint32_t atlas_layer);
uint64_t pack_empty_world_water_face_with_layer(
    uint32_t x, uint32_t y, uint32_t z, uint32_t direction, uint32_t span_u,
    uint32_t span_v, uint32_t atlas_layer, uint32_t water_level,
    uint32_t base_height);
uint32_t unpack_empty_world_face_x(uint64_t face);
uint32_t unpack_empty_world_face_y(uint64_t face);
uint32_t unpack_empty_world_face_z(uint64_t face);
uint32_t unpack_empty_world_face_direction(uint64_t face);
uint32_t unpack_empty_world_face_span_u(uint64_t face);
uint32_t unpack_empty_world_face_span_v(uint64_t face);
int32_t floor_div_int32(int32_t value, int32_t divisor);
int32_t floor_mod_int32(int32_t value, int32_t divisor);
