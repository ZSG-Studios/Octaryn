#include "SlangRhiVoxelPrefixRanges.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_prefix_ranges();

  if (!result.readback_valid) {
    std::fprintf(stderr,
                 "client voxel prefix range probe failed: status=%s device=%d "
                 "queue=%d heap=%d pipelines=%d command=%d buffers=%d "
                 "bound=%d face_dispatch=%d greedy_dispatch=%d "
                 "prefix_dispatch=%d submitted=%d fence=%d readback=%d "
                 "empty_offset=%u uniform_offset=%u checkerboard_offset=%u "
                 "mixed_offset=%u total=%u\n",
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
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.readback_valid ? 1 : 0, result.empty_offset,
                 result.uniform_offset, result.checkerboard_offset,
                 result.mixed_offset, result.total_quads);
    return 1;
  }

  std::printf("client_voxel_prefix_range_probe=passed status=%s "
              "empty_offset=%u uniform_offset=%u checkerboard_offset=%u "
              "mixed_offset=%u total=%u face_dispatch=1 greedy_dispatch=1 "
              "prefix_dispatch=1 readback=1\n",
              result.status, result.empty_offset, result.uniform_offset,
              result.checkerboard_offset, result.mixed_offset,
              result.total_quads);
  return 0;
}
