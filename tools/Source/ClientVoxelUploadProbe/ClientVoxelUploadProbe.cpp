#include "SlangRhiVoxelUpload.h"

#include <cstdio>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_voxel_palette_uploads();

  if (!result.headers_readback_valid || !result.palette_readback_valid ||
      !result.payload_readback_valid || !result.empty_readback_valid ||
      !result.uniform_readback_valid || !result.mixed_readback_valid) {
    std::fprintf(stderr,
                 "client voxel upload probe failed: status=%s device=%d "
                 "queue=%d heap=%d command=%d empty=%d uniform=%d mixed=%d "
                 "headers=%d palette=%d payload=%d submitted=%d fence=%d "
                 "empty_readback=%d "
                 "uniform_readback=%d mixed_readback=%d\n",
                 result.status, result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.transient_heap_created ? 1 : 0,
                 result.command_buffer_created ? 1 : 0,
                 result.empty_uploaded ? 1 : 0,
                 result.uniform_uploaded ? 1 : 0,
                 result.mixed_uploaded ? 1 : 0,
                 result.headers_readback_valid ? 1 : 0,
                 result.palette_readback_valid ? 1 : 0,
                 result.payload_readback_valid ? 1 : 0,
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.empty_readback_valid ? 1 : 0,
                 result.uniform_readback_valid ? 1 : 0,
                 result.mixed_readback_valid ? 1 : 0);
    return 1;
  }

  std::printf("client_voxel_upload_probe=passed status=%s headers=1 "
              "palette=1 payload=1 empty=1 uniform=1 mixed=1 readback=1\n",
              result.status);
  return 0;
}
