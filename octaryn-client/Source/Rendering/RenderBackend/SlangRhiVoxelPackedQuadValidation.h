#pragma once

#include "PackedVoxelQuad.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::rendering {

bool slang_rhi_voxel_packed_quad_ranges_match(
    const std::vector<octaryn::client::voxel::PackedVoxelQuad16> &quads,
    std::uint32_t offset, std::uint32_t count,
    std::uint32_t expected_material_sum);

} // namespace octaryn::client::rendering
