#include "SlangRhiVoxelIndirectGeneration.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_indirect_generation();

  if (!result.readback_valid) {
    std::fprintf(stderr,
                 "client voxel indirect generation probe failed: status=%s "
                 "device=%d queue=%d heap=%d pipelines=%d command=%d "
                 "buffers=%d bound=%d face_dispatch=%d greedy_dispatch=%d "
                 "prefix_dispatch=%d emit_dispatch=%d indirect_dispatch=%d "
                 "submitted=%d fence=%d readback=%d draws=%u empty_draws=%u "
                 "instances=%u first_uniform=%u first_checkerboard=%u "
                 "first_mixed=%u checksum_nonzero=%u\n",
                 result.status, result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.transient_heap_created ? 1 : 0,
                 result.pipelines_created ? 1 : 0,
                 result.command_buffer_created ? 1 : 0,
                 result.buffers_created ? 1 : 0,
                 result.resources_bound ? 1 : 0,
                 result.face_masks_dispatched ? 1 : 0,
                 result.greedy_dispatched ? 1 : 0,
                 result.prefix_dispatched ? 1 : 0,
                 result.emit_dispatched ? 1 : 0,
                 result.indirect_dispatched ? 1 : 0,
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.readback_valid ? 1 : 0, result.draw_count,
                 result.empty_draw_count, result.total_instances,
                 result.first_instance_uniform,
                 result.first_instance_checkerboard,
                 result.first_instance_mixed, result.checksum_nonzero);
    return 1;
  }

  std::printf("client_voxel_indirect_generation_probe=passed status=%s "
              "draws=%u empty_draws=%u instances=%u first_uniform=%u "
              "first_checkerboard=%u first_mixed=%u checksum_nonzero=%u "
              "face_dispatch=1 greedy_dispatch=1 prefix_dispatch=1 "
              "emit_dispatch=1 indirect_dispatch=1 readback=1\n",
              result.status, result.draw_count, result.empty_draw_count,
              result.total_instances, result.first_instance_uniform,
              result.first_instance_checkerboard, result.first_instance_mixed,
              result.checksum_nonzero);
  return 0;
}
