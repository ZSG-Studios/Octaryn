#include "SlangRhiVoxelIndirectValidation.h"

namespace octaryn::client::rendering {

bool slang_rhi_voxel_indirect_commands_match(
    const std::vector<octaryn::client::voxel::DrawIndexedIndirectCommand>
        &commands,
    std::uint32_t draw_count, std::uint32_t empty_draw_count,
    std::uint32_t total_instances, std::uint32_t checksum) {
  if (commands.size() < 3u || draw_count != 3u || empty_draw_count != 1u ||
      total_instances != 98440u || checksum == 0u) {
    return false;
  }
  return commands[0].index_count == 6u &&
         commands[0].instance_count == 6u &&
         commands[0].first_instance == 0u &&
         commands[1].index_count == 6u &&
         commands[1].instance_count == 98304u &&
         commands[1].first_instance == 6u &&
         commands[2].index_count == 6u &&
         commands[2].instance_count == 130u &&
         commands[2].first_instance == 98310u;
}

} // namespace octaryn::client::rendering
