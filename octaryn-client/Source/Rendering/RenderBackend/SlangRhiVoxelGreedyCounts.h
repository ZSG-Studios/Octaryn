#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelGreedyCountProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool programs_created;
  bool pipelines_created;
  bool command_buffer_created;
  bool buffers_created;
  bool resources_bound;
  bool face_masks_dispatched;
  bool greedy_dispatched;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  unsigned empty_quads;
  unsigned uniform_quads;
  unsigned checkerboard_quads;
  unsigned mixed_quads;
  unsigned empty_material_sum;
  unsigned uniform_material_sum;
  unsigned checkerboard_material_sum;
  unsigned mixed_material_sum;
  const char *status;
};

SlangRhiVoxelGreedyCountProbeResult probe_slang_rhi_voxel_greedy_counts();

} // namespace octaryn::client::rendering
