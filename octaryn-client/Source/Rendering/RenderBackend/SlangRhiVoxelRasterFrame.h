#pragma once

#include <cstdint>

namespace octaryn::client::rendering {

struct SlangRhiVoxelRasterStreamBatch;
struct SlangRhiVoxelRasterFrameSession;

struct SlangRhiVoxelRasterFrameProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool compute_pipelines_created;
  bool graphics_pipeline_created;
  bool command_buffer_created;
  bool buffers_created;
  bool resources_bound;
  bool compute_dispatched;
  bool indirect_draw_encoded;
  bool submitted;
  bool fence_completed;
  bool color_readback_valid;
  bool retained_frame_resources;
  bool session_reused;
  bool live_stream_source;
  unsigned draw_count;
  unsigned total_instances;
  unsigned non_clear_pixels;
  unsigned bounded_columns;
  unsigned live_columns;
  unsigned frames_rendered;
  unsigned stream_updates;
  std::uint64_t retained_gpu_bytes;
  std::uint64_t upload_staging_bytes;
  int stream_center_x;
  int stream_center_z;
  int first_column_x;
  int first_column_z;
  const char *status;
};

struct SlangRhiVoxelRasterFrameTiming {
  std::uint64_t stream_upload_microseconds;
  std::uint64_t pass_graph_microseconds;
  std::uint64_t draw_microseconds;
  std::uint64_t submit_readback_microseconds;
  std::uint64_t frame_microseconds;
};

struct SlangRhiVoxelRasterFrameRenderResult {
  SlangRhiVoxelRasterFrameProbeResult probe;
  SlangRhiVoxelRasterFrameTiming timing;
};

SlangRhiVoxelRasterFrameSession *
create_slang_rhi_voxel_raster_frame_session(
    SlangRhiVoxelRasterFrameProbeResult &probe);

void destroy_slang_rhi_voxel_raster_frame_session(
    SlangRhiVoxelRasterFrameSession *session);

SlangRhiVoxelRasterFrameRenderResult
render_slang_rhi_voxel_raster_session_frame(
    SlangRhiVoxelRasterFrameSession *session,
    const SlangRhiVoxelRasterStreamBatch *updated_batch,
    SlangRhiVoxelRasterFrameProbeResult &probe);

SlangRhiVoxelRasterFrameProbeResult render_slang_rhi_voxel_raster_frame();
SlangRhiVoxelRasterFrameProbeResult probe_slang_rhi_voxel_raster_frame();

} // namespace octaryn::client::rendering
