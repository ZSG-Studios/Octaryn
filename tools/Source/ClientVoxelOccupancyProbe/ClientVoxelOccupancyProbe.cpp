#include "SlangRhiVoxelOccupancy.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_occupancy_decode();

  if (!result.readback_valid) {
    std::fprintf(stderr,
                 "client voxel occupancy probe failed: status=%s device=%d "
                 "queue=%d heap=%d program=%d pipeline=%d command=%d "
                 "buffers=%d bound=%d dispatched=%d submitted=%d fence=%d "
                 "readback=%d empty=%u uniform=%u mixed=%u\n",
                 result.status, result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.transient_heap_created ? 1 : 0,
                 result.program_created ? 1 : 0,
                 result.pipeline_created ? 1 : 0,
                 result.command_buffer_created ? 1 : 0,
                 result.buffers_created ? 1 : 0,
                 result.resources_bound ? 1 : 0,
                 result.dispatched ? 1 : 0,
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.readback_valid ? 1 : 0,
                 result.empty_occupancy,
                 result.uniform_occupancy,
                 result.mixed_occupancy);
    return 1;
  }

  std::printf("client_voxel_occupancy_probe=passed status=%s empty=%u "
              "uniform=%u mixed=%u dispatched=1 readback=1\n",
              result.status,
              result.empty_occupancy,
              result.uniform_occupancy,
              result.mixed_occupancy);
  return 0;
}
