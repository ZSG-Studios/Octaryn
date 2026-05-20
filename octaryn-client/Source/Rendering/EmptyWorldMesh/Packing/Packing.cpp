#include "Packing.h"

namespace {

constexpr uint32_t kPackedFaceXOffset = 0u;
constexpr uint32_t kPackedFaceYOffset = 5u;
constexpr uint32_t kPackedFaceZOffset = 13u;
constexpr uint32_t kPackedFaceDirectionOffset = 18u;
constexpr uint32_t kPackedFaceSpanUOffset = 21u;
constexpr uint32_t kPackedFaceSpanVOffset = 29u;
constexpr uint32_t kPackedFaceAtlasLayerOffset = 37u;
constexpr uint32_t kPackedFaceOcclusionOffset = 43u;
constexpr uint32_t kPackedFaceChunkSlotOffset = 44u;
constexpr uint32_t kPackedFaceWaterLevelOffset = 57u;
constexpr uint32_t kPackedFaceWaterFlagOffset = 60u;
constexpr uint32_t kPackedFaceWaterBaseHeightOffset = 61u;
constexpr uint64_t kPackedFaceUnsetChunkSlot = 0x1fffu;

uint64_t pack_empty_world_face_field(uint64_t packed, uint64_t value,
                                     uint32_t offset, uint64_t mask) {
  return packed | ((value & mask) << offset);
}

} // namespace

uint64_t pack_empty_world_block_face(uint32_t x, uint32_t y, uint32_t z,
                                     uint32_t direction, uint32_t span_u,
                                     uint32_t span_v) {
  return pack_empty_world_block_face_with_layer(x, y, z, direction, span_u,
                                                span_v, 0u);
}

uint64_t pack_empty_world_block_face_with_layer(
    uint32_t x, uint32_t y, uint32_t z, uint32_t direction, uint32_t span_u,
    uint32_t span_v, uint32_t atlas_layer) {
  uint64_t packed = 0u;
  packed = pack_empty_world_face_field(packed, x, kPackedFaceXOffset, 0x1fu);
  packed = pack_empty_world_face_field(packed, y, kPackedFaceYOffset, 0xffu);
  packed = pack_empty_world_face_field(packed, z, kPackedFaceZOffset, 0x1fu);
  packed = pack_empty_world_face_field(packed, direction,
                                       kPackedFaceDirectionOffset, 0x7u);
  packed = pack_empty_world_face_field(packed, span_u - 1u,
                                       kPackedFaceSpanUOffset, 0xffu);
  packed = pack_empty_world_face_field(packed, span_v - 1u,
                                       kPackedFaceSpanVOffset, 0xffu);
  packed = pack_empty_world_face_field(packed, atlas_layer,
                                       kPackedFaceAtlasLayerOffset, 0x3fu);
  packed =
      pack_empty_world_face_field(packed, 1u, kPackedFaceOcclusionOffset, 0x1u);
  packed = pack_empty_world_face_field(packed, kPackedFaceUnsetChunkSlot,
                                       kPackedFaceChunkSlotOffset, 0x1fffu);
  packed = pack_empty_world_face_field(packed, 0u, kPackedFaceWaterLevelOffset,
                                       0x7u);
  packed =
      pack_empty_world_face_field(packed, 0u, kPackedFaceWaterFlagOffset, 0x1u);
  packed = pack_empty_world_face_field(packed, 0u,
                                       kPackedFaceWaterBaseHeightOffset, 0x7u);
  return packed;
}

uint64_t pack_empty_world_water_face_with_layer(
    uint32_t x, uint32_t y, uint32_t z, uint32_t direction, uint32_t span_u,
    uint32_t span_v, uint32_t atlas_layer, uint32_t water_level,
    uint32_t base_height) {
  uint64_t packed = pack_empty_world_block_face_with_layer(
      x, y, z, direction, span_u, span_v, atlas_layer);
  packed = pack_empty_world_face_field(packed, water_level,
                                       kPackedFaceWaterLevelOffset, 0x7u);
  packed =
      pack_empty_world_face_field(packed, 1u, kPackedFaceWaterFlagOffset, 0x1u);
  packed = pack_empty_world_face_field(packed, base_height,
                                       kPackedFaceWaterBaseHeightOffset, 0x7u);
  return packed;
}

uint32_t unpack_empty_world_face_x(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceXOffset) & 0x1fu);
}

uint32_t unpack_empty_world_face_y(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceYOffset) & 0xffu);
}

uint32_t unpack_empty_world_face_z(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceZOffset) & 0x1fu);
}

uint32_t unpack_empty_world_face_direction(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceDirectionOffset) & 0x7u);
}

uint32_t unpack_empty_world_face_span_u(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceSpanUOffset) & 0xffu) + 1u;
}

uint32_t unpack_empty_world_face_span_v(uint64_t face) {
  return static_cast<uint32_t>((face >> kPackedFaceSpanVOffset) & 0xffu) + 1u;
}

int32_t floor_div_int32(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

int32_t floor_mod_int32(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}
