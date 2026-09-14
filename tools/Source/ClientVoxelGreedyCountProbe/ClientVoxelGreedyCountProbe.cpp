#include "SlangRhiVoxelGreedyCounts.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_greedy_counts();

  if (!result.readback_valid) {
    std::fprintf(stderr,
                 "client voxel greedy count probe failed: status=%s device=%d "
                 "queue=%d heap=%d programs=%d pipelines=%d command=%d "
                 "buffers=%d bound=%d face_dispatch=%d greedy_dispatch=%d "
                 "submitted=%d fence=%d readback=%d empty=%u uniform=%u "
                 "checkerboard=%u mixed=%u empty_material=%u "
                 "uniform_material=%u checkerboard_material=%u "
                 "mixed_material=%u\n",
                 result.status, result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.transient_heap_created ? 1 : 0,
                 result.programs_created ? 1 : 0,
                 result.pipelines_created ? 1 : 0,
                 result.command_buffer_created ? 1 : 0,
                 result.buffers_created ? 1 : 0,
                 result.resources_bound ? 1 : 0,
                 result.face_masks_dispatched ? 1 : 0,
                 result.greedy_dispatched ? 1 : 0,
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.readback_valid ? 1 : 0, result.empty_quads,
                 result.uniform_quads, result.checkerboard_quads,
                 result.mixed_quads, result.empty_material_sum,
                 result.uniform_material_sum, result.checkerboard_material_sum,
                 result.mixed_material_sum);
    return 1;
  }

  std::printf("client_voxel_greedy_count_probe=passed status=%s empty=%u "
              "uniform=%u checkerboard=%u mixed=%u empty_material=%u "
              "uniform_material=%u checkerboard_material=%u "
              "mixed_material=%u face_dispatch=1 greedy_dispatch=1 readback=1\n",
              result.status, result.empty_quads, result.uniform_quads,
              result.checkerboard_quads, result.mixed_quads,
              result.empty_material_sum, result.uniform_material_sum,
              result.checkerboard_material_sum, result.mixed_material_sum);
  return 0;
}
