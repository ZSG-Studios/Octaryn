#pragma once

#include "VoxelIndirect.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::rendering {

bool slang_rhi_voxel_indirect_commands_match(
    const std::vector<octaryn::client::voxel::DrawIndexedIndirectCommand>
        &commands,
    std::uint32_t draw_count, std::uint32_t empty_draw_count,
    std::uint32_t total_instances, std::uint32_t checksum);

} // namespace octaryn::client::rendering
