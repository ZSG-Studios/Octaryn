#pragma once

#include <cstdint>

namespace octaryn::client::voxel {

struct DrawIndexedIndirectCommand {
  std::uint32_t index_count;
  std::uint32_t instance_count;
  std::uint32_t first_index;
  std::int32_t vertex_offset;
  std::uint32_t first_instance;
};

bool indirect_command_valid(const DrawIndexedIndirectCommand &command) noexcept;
DrawIndexedIndirectCommand make_voxel_quad_indirect_command(
    std::uint32_t quad_base, std::uint32_t quad_count,
    std::uint32_t unit_quad_index_offset = 0u) noexcept;

} // namespace octaryn::client::voxel
