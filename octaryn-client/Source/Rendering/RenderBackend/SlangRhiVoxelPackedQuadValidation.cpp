#include "SlangRhiVoxelPackedQuadValidation.h"

namespace octaryn::client::rendering {
namespace {

constexpr std::uint32_t MaterialMask = (1u << 20u) - 1u;

} // namespace

bool slang_rhi_voxel_packed_quad_ranges_match(
    const std::vector<octaryn::client::voxel::PackedVoxelQuad16> &quads,
    std::uint32_t offset, std::uint32_t count,
    std::uint32_t expected_material_sum) {
  std::uint64_t material_sum = 0u;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto &quad = quads[offset + i];
    const std::uint32_t x = quad.pos_size_dir_lod & 31u;
    const std::uint32_t y = (quad.pos_size_dir_lod >> 5u) & 31u;
    const std::uint32_t z = (quad.pos_size_dir_lod >> 10u) & 31u;
    const std::uint32_t direction = (quad.pos_size_dir_lod >> 25u) & 7u;
    if (x >= 32u || y >= 32u || z >= 32u || direction >= 6u) {
      return false;
    }
    material_sum += quad.material_flags & MaterialMask;
  }
  return material_sum == expected_material_sum;
}

} // namespace octaryn::client::rendering
