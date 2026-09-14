#include "VoxelFaceMask.h"

namespace octaryn::client::voxel {
namespace {

struct DirectionOffset {
  std::int32_t x;
  std::int32_t y;
  std::int32_t z;
};

DirectionOffset direction_offset(FaceDirection direction) noexcept {
  switch (direction) {
  case FaceDirection::PosX:
    return {1, 0, 0};
  case FaceDirection::NegX:
    return {-1, 0, 0};
  case FaceDirection::PosY:
    return {0, 1, 0};
  case FaceDirection::NegY:
    return {0, -1, 0};
  case FaceDirection::PosZ:
    return {0, 0, 1};
  case FaceDirection::NegZ:
    return {0, 0, -1};
  }
  return {0, 0, 0};
}

} // namespace

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) noexcept {
  return x + ChunkWidthBlocks * (y + ChunkWidthBlocks * z);
}

bool voxel_coord_valid(std::int32_t x, std::int32_t y,
                       std::int32_t z) noexcept {
  return x >= 0 && x < ChunkWidthBlocks && y >= 0 && y < ChunkWidthBlocks &&
         z >= 0 && z < ChunkWidthBlocks;
}

void set_voxel_occupied(VoxelMaskChunk &chunk, std::uint32_t x, std::uint32_t y,
                        std::uint32_t z, bool occupied) noexcept {
  chunk.occupied[voxel_index(x, y, z)] = occupied ? 1u : 0u;
}

bool voxel_occupied(const VoxelMaskChunk &chunk, std::int32_t x, std::int32_t y,
                    std::int32_t z) noexcept {
  if (!voxel_coord_valid(x, y, z)) {
    return false;
  }
  return chunk.occupied[voxel_index(static_cast<std::uint32_t>(x),
                                    static_cast<std::uint32_t>(y),
                                    static_cast<std::uint32_t>(z))] != 0u;
}

bool voxel_face_visible(const VoxelMaskChunk &chunk, std::uint32_t x,
                        std::uint32_t y, std::uint32_t z,
                        FaceDirection direction) noexcept {
  if (!voxel_occupied(chunk, static_cast<std::int32_t>(x),
                      static_cast<std::int32_t>(y),
                      static_cast<std::int32_t>(z))) {
    return false;
  }
  const auto offset = direction_offset(direction);
  return !voxel_occupied(chunk, static_cast<std::int32_t>(x) + offset.x,
                         static_cast<std::int32_t>(y) + offset.y,
                         static_cast<std::int32_t>(z) + offset.z);
}

std::uint32_t count_visible_faces(const VoxelMaskChunk &chunk) noexcept {
  std::uint32_t faces = 0;
  for (std::uint32_t z = 0; z < ChunkWidthBlocks; ++z) {
    for (std::uint32_t y = 0; y < ChunkWidthBlocks; ++y) {
      for (std::uint32_t x = 0; x < ChunkWidthBlocks; ++x) {
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::PosX) ? 1u : 0u;
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::NegX) ? 1u : 0u;
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::PosY) ? 1u : 0u;
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::NegY) ? 1u : 0u;
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::PosZ) ? 1u : 0u;
        faces +=
            voxel_face_visible(chunk, x, y, z, FaceDirection::NegZ) ? 1u : 0u;
      }
    }
  }
  return faces;
}

VoxelMaskChunk make_uniform_solid_chunk() noexcept {
  VoxelMaskChunk chunk{};
  chunk.occupied.fill(1u);
  return chunk;
}

VoxelMaskChunk make_checkerboard_chunk() noexcept {
  VoxelMaskChunk chunk{};
  for (std::uint32_t z = 0; z < ChunkWidthBlocks; ++z) {
    for (std::uint32_t y = 0; y < ChunkWidthBlocks; ++y) {
      for (std::uint32_t x = 0; x < ChunkWidthBlocks; ++x) {
        set_voxel_occupied(chunk, x, y, z, ((x + y + z) % 2u) == 0u);
      }
    }
  }
  return chunk;
}

} // namespace octaryn::client::voxel
