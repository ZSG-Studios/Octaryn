#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelUploadProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool command_buffer_created;
  bool empty_uploaded;
  bool uniform_uploaded;
  bool mixed_uploaded;
  bool headers_readback_valid;
  bool palette_readback_valid;
  bool payload_readback_valid;
  bool submitted;
  bool fence_completed;
  bool empty_readback_valid;
  bool uniform_readback_valid;
  bool mixed_readback_valid;
  const char *status;
};

SlangRhiVoxelUploadProbeResult probe_slang_rhi_voxel_palette_uploads();

} // namespace octaryn::client::rendering
