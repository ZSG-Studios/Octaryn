#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelPackedQuadEmitProbeResult {
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
  bool emit_dispatched;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  unsigned empty_count;
  unsigned uniform_count;
  unsigned checkerboard_count;
  unsigned mixed_count;
  unsigned total_quads;
  unsigned uniform_material_sum;
  unsigned checkerboard_material_sum;
  unsigned mixed_material_sum;
  unsigned nonzero_checksums;
  const char *status;
};

SlangRhiVoxelPackedQuadEmitProbeResult
probe_slang_rhi_voxel_packed_quad_emit();

} // namespace octaryn::client::rendering
