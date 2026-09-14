#include "FrameLoop.h"

#include "VoxelRasterFrameLoop.h"

#include <cstdio>

namespace octaryn::client::app {
namespace {

void write_line(FILE *log, const char *line) {
  std::puts(line);
  if (log != nullptr) {
    std::fprintf(log, "%s\n", line);
    std::fflush(log);
  }
}

void write_voxel_raster_line(
    FILE *log,
    const octaryn::client::world_presentation::VoxelRasterFrameLoopResult
        &voxel_loop) {
  const auto &voxel_frame = voxel_loop.raster;
  char voxel_line[512]{};
  std::snprintf(voxel_line, sizeof(voxel_line),
                "client_voxel_raster_runtime status=%s columns=%u "
                "live_columns=%u "
                "center=(%d,%d) first_column=(%d,%d) "
                "stream_source=%s "
                "retained_resources=%d session_reused=%d frames=%u "
                "updates=%u draws=%u instances=%u nonclear=%u "
                "compute_dispatch=%d indirect_draw=%d readback=%d "
                "retained_gpu_bytes=%llu upload_staging_bytes=%llu",
                voxel_frame.status, voxel_frame.bounded_columns,
                voxel_frame.live_columns, voxel_frame.stream_center_x,
                voxel_frame.stream_center_z, voxel_frame.first_column_x,
                voxel_frame.first_column_z,
                voxel_frame.live_stream_source ? "live_sidecar"
                                               : "fallback_fixture",
                voxel_frame.retained_frame_resources ? 1 : 0,
                voxel_frame.session_reused ? 1 : 0,
                voxel_frame.frames_rendered, voxel_frame.stream_updates,
                voxel_frame.draw_count, voxel_frame.total_instances,
                voxel_frame.non_clear_pixels,
                voxel_frame.compute_dispatched ? 1 : 0,
                voxel_frame.indirect_draw_encoded ? 1 : 0,
                voxel_frame.color_readback_valid ? 1 : 0,
                static_cast<unsigned long long>(
                    voxel_frame.retained_gpu_bytes),
                static_cast<unsigned long long>(
                    voxel_frame.upload_staging_bytes));
  write_line(log, voxel_line);
}

void write_voxel_frame_loop_line(
    FILE *log,
    const octaryn::client::world_presentation::VoxelRasterFrameLoopResult
        &voxel_loop) {
  const auto &voxel_frame = voxel_loop.raster;
  char voxel_metrics_line[768]{};
  std::snprintf(voxel_metrics_line, sizeof(voxel_metrics_line),
                "client_voxel_world_frame_loop requested_frames=%u "
                "frames=%u batches=%u live_columns=%u retained_columns=%u "
                "retained_chunks=%u radius32_stream_available=%d elapsed_us=%llu "
                "session_create_us=%llu stream_build_us=%llu "
                "upload_us=%llu pass_graph_us=%llu draw_us=%llu "
                "submit_readback_us=%llu updates=%u draws=%u instances=%u "
                "stream_source=%s indirect_draw=%d readback=%d "
                "retained_gpu_bytes=%llu upload_staging_bytes=%llu",
                voxel_loop.requested_frames, voxel_frame.frames_rendered,
                voxel_loop.stream_batches, voxel_loop.live_columns,
                voxel_loop.retained_columns, voxel_loop.retained_chunks,
                voxel_loop.radius32_stream_available ? 1 : 0,
                static_cast<unsigned long long>(
                    voxel_loop.elapsed_microseconds),
                static_cast<unsigned long long>(
                    voxel_loop.session_create_microseconds),
                static_cast<unsigned long long>(
                    voxel_loop.stream_build_microseconds),
                static_cast<unsigned long long>(voxel_loop.upload_microseconds),
                static_cast<unsigned long long>(
                    voxel_loop.pass_graph_microseconds),
                static_cast<unsigned long long>(voxel_loop.draw_microseconds),
                static_cast<unsigned long long>(
                    voxel_loop.submit_readback_microseconds),
                voxel_frame.stream_updates, voxel_frame.draw_count,
                voxel_frame.total_instances,
                voxel_frame.live_stream_source ? "live_sidecar"
                                               : "fallback_fixture",
                voxel_frame.indirect_draw_encoded ? 1 : 0,
                voxel_frame.color_readback_valid ? 1 : 0,
                static_cast<unsigned long long>(
                    voxel_loop.retained_gpu_bytes),
                static_cast<unsigned long long>(
                    voxel_loop.upload_staging_bytes));
  write_line(log, voxel_metrics_line);
}

} // namespace

bool run_frame_loop(FILE *log) {
  auto *voxel_loop_state =
      octaryn::client::world_presentation::
          create_voxel_raster_world_frame_loop();
  unsigned frames_rendered = 0u;
  for (; frames_rendered < VoxelRuntimeFrames; ++frames_rendered) {
    if (!octaryn::client::world_presentation::render_voxel_raster_world_frame(
            voxel_loop_state)) {
      break;
    }
  }

  char app_frame_line[128]{};
  std::snprintf(app_frame_line, sizeof(app_frame_line),
                "client_app_frame_loop frames=%u requested_frames=%u "
                "retained_voxel_session=1",
                frames_rendered, VoxelRuntimeFrames);
  write_line(log, app_frame_line);

  const auto voxel_loop =
      octaryn::client::world_presentation::
          snapshot_voxel_raster_world_frame_loop(voxel_loop_state,
                                                 VoxelRuntimeFrames);
  octaryn::client::world_presentation::destroy_voxel_raster_world_frame_loop(
      voxel_loop_state);

  write_voxel_raster_line(log, voxel_loop);
  write_voxel_frame_loop_line(log, voxel_loop);
  return frames_rendered == VoxelRuntimeFrames &&
         voxel_loop.raster.color_readback_valid;
}

} // namespace octaryn::client::app
