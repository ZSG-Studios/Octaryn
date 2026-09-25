#pragma once

#include "PackedVoxelQuad.h"
#include "VoxelFaceMask.h"

#include <vector>

namespace octaryn::client::voxel {

std::vector<PackedVoxelQuad16>
build_uniform_solid_shell_quads(std::uint32_t material_id) noexcept;
bool solid_shell_quads_valid(const std::vector<PackedVoxelQuad16> &quads,
                             std::uint32_t material_id) noexcept;

} // namespace octaryn::client::voxel
