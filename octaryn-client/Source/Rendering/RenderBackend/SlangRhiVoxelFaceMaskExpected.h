#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace octaryn::client::rendering {

constexpr std::uint32_t FaceMaskProbeChunkCount = 4u;

struct SlangRhiVoxelFaceMaskExpected {
  std::vector<std::uint32_t> masks;
  std::array<std::uint32_t, FaceMaskProbeChunkCount> counts;
};

SlangRhiVoxelFaceMaskExpected build_slang_rhi_voxel_face_mask_expected();

} // namespace octaryn::client::rendering
