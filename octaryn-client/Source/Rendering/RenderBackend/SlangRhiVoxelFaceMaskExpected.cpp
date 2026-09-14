#include "SlangRhiVoxelFaceMaskExpected.h"

#include "VoxelLayout.h"

namespace octaryn::client::rendering {
namespace {

constexpr std::uint32_t UniformChunk = 1u;
constexpr std::uint32_t CheckerboardChunk = 2u;
constexpr std::uint32_t MixedChunk = 3u;

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  return x + W * (y + W * z);
}

std::uint32_t expected_block_id(std::uint32_t chunk, std::int32_t x,
                                std::int32_t y, std::int32_t z) {
  constexpr auto W = static_cast<std::int32_t>(
      octaryn::client::voxel::ChunkWidthBlocks);
  if (x < 0 || y < 0 || z < 0 || x >= W || y >= W || z >= W) {
    return 0u;
  }
  if (chunk == UniformChunk) {
    return 42u;
  }
  if (chunk == CheckerboardChunk) {
    return ((x + y + z) & 1) == 0 ? 37u : 0u;
  }
  if (chunk == MixedChunk) {
    const std::uint32_t palette_index = static_cast<std::uint32_t>(x) & 3u;
    return 20u + palette_index;
  }
  return 0u;
}

std::uint32_t expected_mask_at(std::uint32_t chunk, std::uint32_t x,
                               std::uint32_t y, std::uint32_t z) {
  const auto sx = static_cast<std::int32_t>(x);
  const auto sy = static_cast<std::int32_t>(y);
  const auto sz = static_cast<std::int32_t>(z);
  if (expected_block_id(chunk, sx, sy, sz) == 0u) {
    return 0u;
  }
  std::uint32_t mask = 0u;
  mask |= expected_block_id(chunk, sx + 1, sy, sz) == 0u ? 1u : 0u;
  mask |= expected_block_id(chunk, sx - 1, sy, sz) == 0u ? 2u : 0u;
  mask |= expected_block_id(chunk, sx, sy + 1, sz) == 0u ? 4u : 0u;
  mask |= expected_block_id(chunk, sx, sy - 1, sz) == 0u ? 8u : 0u;
  mask |= expected_block_id(chunk, sx, sy, sz + 1) == 0u ? 16u : 0u;
  mask |= expected_block_id(chunk, sx, sy, sz - 1) == 0u ? 32u : 0u;
  return mask;
}

std::uint32_t face_count(std::uint32_t mask) {
  std::uint32_t count = 0u;
  for (std::uint32_t bit = 0u; bit < 6u; ++bit) {
    count += (mask >> bit) & 1u;
  }
  return count;
}

} // namespace

SlangRhiVoxelFaceMaskExpected build_slang_rhi_voxel_face_mask_expected() {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  SlangRhiVoxelFaceMaskExpected expected{};
  expected.masks.assign(FaceMaskProbeChunkCount *
                            octaryn::client::voxel::ChunkVoxelCount,
                        0u);
  for (std::uint32_t chunk = 0; chunk < FaceMaskProbeChunkCount; ++chunk) {
    for (std::uint32_t z = 0; z < W; ++z) {
      for (std::uint32_t y = 0; y < W; ++y) {
        for (std::uint32_t x = 0; x < W; ++x) {
          const std::uint32_t mask = expected_mask_at(chunk, x, y, z);
          const std::uint32_t index =
              chunk * octaryn::client::voxel::ChunkVoxelCount +
              voxel_index(x, y, z);
          expected.masks[index] = mask;
          expected.counts[chunk] += face_count(mask);
        }
      }
    }
  }
  return expected;
}

} // namespace octaryn::client::rendering
