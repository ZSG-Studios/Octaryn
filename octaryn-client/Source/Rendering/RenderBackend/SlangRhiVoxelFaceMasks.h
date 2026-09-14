#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelFaceMaskProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool program_created;
  bool pipeline_created;
  bool command_buffer_created;
  bool buffers_created;
  bool resources_bound;
  bool dispatched;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  unsigned empty_faces;
  unsigned uniform_faces;
  unsigned checkerboard_faces;
  unsigned mixed_faces;
  const char *status;
};

SlangRhiVoxelFaceMaskProbeResult probe_slang_rhi_voxel_face_masks();

} // namespace octaryn::client::rendering
