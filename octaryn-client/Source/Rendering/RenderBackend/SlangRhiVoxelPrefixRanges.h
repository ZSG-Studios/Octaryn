#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelPrefixRangeProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool pipelines_created;
  bool command_buffer_created;
  bool buffers_created;
  bool resources_bound;
  bool face_masks_dispatched;
  bool greedy_dispatched;
  bool prefix_dispatched;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  unsigned empty_offset;
  unsigned uniform_offset;
  unsigned checkerboard_offset;
  unsigned mixed_offset;
  unsigned total_quads;
  const char *status;
};

SlangRhiVoxelPrefixRangeProbeResult probe_slang_rhi_voxel_prefix_ranges();

} // namespace octaryn::client::rendering
