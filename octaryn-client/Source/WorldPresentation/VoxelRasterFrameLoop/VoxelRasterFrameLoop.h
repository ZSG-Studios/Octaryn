#pragma once

#include "SlangRhiVoxelRasterFrame.h"

#include <cstdint>

namespace octaryn::client::world_presentation {

struct VoxelRasterFrameLoopState;

struct VoxelRasterFrameLoopResult {
  octaryn::client::rendering::SlangRhiVoxelRasterFrameProbeResult raster;
  std::uint64_t elapsed_microseconds;
  std::uint32_t requested_frames;
  std::uint32_t stream_batches;
  std::uint32_t live_columns;
  std::uint32_t retained_columns;
  std::uint32_t retained_chunks;
  bool radius32_stream_available;
  std::uint64_t session_create_microseconds;
  std::uint64_t stream_build_microseconds;
  std::uint64_t upload_microseconds;
  std::uint64_t pass_graph_microseconds;
  std::uint64_t draw_microseconds;
  std::uint64_t submit_readback_microseconds;
  std::uint64_t retained_gpu_bytes;
  std::uint64_t upload_staging_bytes;
};

VoxelRasterFrameLoopState *create_voxel_raster_world_frame_loop();
bool render_voxel_raster_world_frame(VoxelRasterFrameLoopState *loop);
VoxelRasterFrameLoopResult
snapshot_voxel_raster_world_frame_loop(const VoxelRasterFrameLoopState *loop,
                                       std::uint32_t requested_frames);
void destroy_voxel_raster_world_frame_loop(VoxelRasterFrameLoopState *loop);

VoxelRasterFrameLoopResult run_voxel_raster_world_frame_loop(
    std::uint32_t requested_frames);

} // namespace octaryn::client::world_presentation
