#pragma once

namespace octaryn::client::rendering {

struct SlangRhiVoxelOccupancyProbeResult {
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
  unsigned empty_occupancy;
  unsigned uniform_occupancy;
  unsigned mixed_occupancy;
  const char *status;
};

SlangRhiVoxelOccupancyProbeResult probe_slang_rhi_voxel_occupancy_decode();

} // namespace octaryn::client::rendering
