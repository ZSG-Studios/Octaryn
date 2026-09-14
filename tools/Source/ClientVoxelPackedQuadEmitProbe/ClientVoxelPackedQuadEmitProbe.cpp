#include "SlangRhiVoxelPackedQuadEmit.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_packed_quad_emit();

  if (!result.readback_valid) {
    std::fprintf(stderr,
                 "client voxel packed quad emit probe failed: status=%s "
                 "device=%d queue=%d heap=%d pipelines=%d command=%d "
                 "buffers=%d bound=%d face_dispatch=%d greedy_dispatch=%d "
                 "prefix_dispatch=%d emit_dispatch=%d submitted=%d fence=%d "
                 "readback=%d empty=%u uniform=%u checkerboard=%u mixed=%u "
                 "total=%u uniform_material=%u checkerboard_material=%u "
                 "mixed_material=%u nonzero_checksums=%u\n",
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
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.readback_valid ? 1 : 0, result.empty_count,
                 result.uniform_count, result.checkerboard_count,
                 result.mixed_count, result.total_quads,
                 result.uniform_material_sum,
                 result.checkerboard_material_sum, result.mixed_material_sum,
                 result.nonzero_checksums);
    return 1;
  }

  std::printf("client_voxel_packed_quad_emit_probe=passed status=%s "
              "empty=%u uniform=%u checkerboard=%u mixed=%u total=%u "
              "uniform_material=%u checkerboard_material=%u mixed_material=%u "
              "nonzero_checksums=%u face_dispatch=1 greedy_dispatch=1 "
              "prefix_dispatch=1 emit_dispatch=1 readback=1\n",
              result.status, result.empty_count, result.uniform_count,
              result.checkerboard_count, result.mixed_count, result.total_quads,
              result.uniform_material_sum, result.checkerboard_material_sum,
              result.mixed_material_sum, result.nonzero_checksums);
  return 0;
}
