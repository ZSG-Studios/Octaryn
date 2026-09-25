#include "VoxelIndirect.h"

namespace octaryn::client::voxel {

bool indirect_command_valid(
    const DrawIndexedIndirectCommand &command) noexcept {
  return command.index_count == 6u && command.instance_count > 0u &&
         command.vertex_offset == 0;
}

DrawIndexedIndirectCommand make_voxel_quad_indirect_command(
    std::uint32_t quad_base, std::uint32_t quad_count,
    std::uint32_t unit_quad_index_offset) noexcept {
  if (quad_count == 0u) {
    return {};
  }
  return {6u, quad_count, unit_quad_index_offset, 0, quad_base};
}

} // namespace octaryn::client::voxel
