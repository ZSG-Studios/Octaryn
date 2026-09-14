#pragma once

#include "VoxelLayout.h"

#include <array>
#include <cstdint>

namespace octaryn::client::voxel {

enum class FaceDirection : std::uint8_t {
  PosX,
  NegX,
  PosY,
  NegY,
  PosZ,
  NegZ,
};

struct VoxelMaskChunk {
  std::array<std::uint8_t, ChunkVoxelCount> occupied{};
};

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) noexcept;
bool voxel_coord_valid(std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void set_voxel_occupied(VoxelMaskChunk &chunk, std::uint32_t x, std::uint32_t y,
                        std::uint32_t z, bool occupied) noexcept;
bool voxel_occupied(const VoxelMaskChunk &chunk, std::int32_t x, std::int32_t y,
                    std::int32_t z) noexcept;
bool voxel_face_visible(const VoxelMaskChunk &chunk, std::uint32_t x,
                        std::uint32_t y, std::uint32_t z,
                        FaceDirection direction) noexcept;
std::uint32_t count_visible_faces(const VoxelMaskChunk &chunk) noexcept;
VoxelMaskChunk make_uniform_solid_chunk() noexcept;
VoxelMaskChunk make_checkerboard_chunk() noexcept;

} // namespace octaryn::client::voxel
