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
  packed = pack_empty_world_face_field(packed, 0u, kPackedFaceAtlasLayerOffset,
                                       0x3fu);
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

int32_t floor_div_int32(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

int32_t floor_mod_int32(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}
