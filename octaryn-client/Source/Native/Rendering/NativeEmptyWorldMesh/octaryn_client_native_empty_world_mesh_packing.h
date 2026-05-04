#pragma once

#include <cstdint>

constexpr int32_t kNativeEmptyWorldChunkSize = 32;
constexpr int32_t kNativeEmptyWorldMinY = -256;
constexpr int32_t kNativeEmptyWorldMaxYExclusive = 256;
constexpr int32_t kNativeEmptyWorldMinChunkY = -8;
constexpr int32_t kNativeEmptyWorldChunkY = -1;
constexpr int32_t kNativeEmptyWorldLocalY = 31;

uint64_t pack_native_empty_block_face(uint32_t x, uint32_t y, uint32_t z,
                                      uint32_t direction, uint32_t span_u,
                                      uint32_t span_v);
int32_t floor_div_int32(int32_t value, int32_t divisor);
int32_t floor_mod_int32(int32_t value, int32_t divisor);
