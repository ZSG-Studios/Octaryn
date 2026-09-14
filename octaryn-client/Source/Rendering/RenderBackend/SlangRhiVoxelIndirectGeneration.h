#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelIndirectGenerationProbeResult {
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
  bool indirect_dispatched;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  unsigned draw_count;
  unsigned empty_draw_count;
  unsigned total_instances;
  unsigned first_instance_uniform;
  unsigned first_instance_checkerboard;
  unsigned first_instance_mixed;
  unsigned checksum_nonzero;
  const char *status;
};

SlangRhiVoxelIndirectGenerationProbeResult
probe_slang_rhi_voxel_indirect_generation();

} // namespace octaryn::client::rendering
