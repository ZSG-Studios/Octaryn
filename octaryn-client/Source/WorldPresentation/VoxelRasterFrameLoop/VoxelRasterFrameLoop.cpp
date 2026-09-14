#include "VoxelRasterFrameLoop.h"

#include "SlangRhiVoxelRasterStreamBatch.h"

#include <chrono>

namespace octaryn::client::world_presentation {
namespace {

using octaryn::client::rendering::SlangRhiVoxelRasterFrameProbeResult;
using octaryn::client::rendering::SlangRhiVoxelRasterFrameSession;
using octaryn::client::rendering::SlangRhiVoxelRasterFrameTiming;
using octaryn::client::voxel::ColumnCoord;

constexpr std::uint32_t Radius32Columns = 4225u;

std::uint64_t elapsed_microseconds(std::chrono::steady_clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start)
          .count());
}

void add_timing(SlangRhiVoxelRasterFrameTiming &total,
                const SlangRhiVoxelRasterFrameTiming &frame) {
  total.stream_upload_microseconds += frame.stream_upload_microseconds;
  total.pass_graph_microseconds += frame.pass_graph_microseconds;
  total.draw_microseconds += frame.draw_microseconds;
  total.submit_readback_microseconds += frame.submit_readback_microseconds;
}

} // namespace

struct VoxelRasterFrameLoopState {
  std::chrono::steady_clock::time_point started;
  SlangRhiVoxelRasterFrameProbeResult raster;
  SlangRhiVoxelRasterFrameSession *session;
  SlangRhiVoxelRasterFrameTiming timing;
  std::uint64_t session_create_microseconds;
  std::uint64_t stream_build_microseconds;
  std::uint32_t stream_batches;
};

VoxelRasterFrameLoopState *create_voxel_raster_world_frame_loop() {
  auto *loop = new VoxelRasterFrameLoopState{};
  loop->started = std::chrono::steady_clock::now();
  const auto session_started = std::chrono::steady_clock::now();
  loop->session =
      octaryn::client::rendering::create_slang_rhi_voxel_raster_frame_session(
          loop->raster);
  loop->session_create_microseconds =
      elapsed_microseconds(session_started);
  return loop;
}

bool render_voxel_raster_world_frame(VoxelRasterFrameLoopState *loop) {
  if (loop == nullptr || loop->session == nullptr) { return false; }
  if (loop->raster.frames_rendered == 0u) {
    const auto render =
        octaryn::client::rendering::render_slang_rhi_voxel_raster_session_frame(
            loop->session, nullptr, loop->raster);
    add_timing(loop->timing, render.timing);
    loop->stream_batches = loop->raster.frames_rendered;
    return render.probe.color_readback_valid;
  }

  const auto build_started = std::chrono::steady_clock::now();
  const auto moved_batch =
      octaryn::client::rendering::
          build_slang_rhi_voxel_raster_stream_batch_for_center(
              ColumnCoord{static_cast<int>(loop->raster.frames_rendered), 0});
  loop->stream_build_microseconds += elapsed_microseconds(build_started);
  const auto render =
      octaryn::client::rendering::render_slang_rhi_voxel_raster_session_frame(
          loop->session, &moved_batch, loop->raster);
  add_timing(loop->timing, render.timing);
  loop->stream_batches = loop->raster.frames_rendered;
  return render.probe.color_readback_valid;
}

VoxelRasterFrameLoopResult
snapshot_voxel_raster_world_frame_loop(const VoxelRasterFrameLoopState *loop,
                                       std::uint32_t requested_frames) {
  if (loop == nullptr) { return {}; }
  SlangRhiVoxelRasterFrameProbeResult raster = loop->raster;
  raster.session_reused = raster.frames_rendered >= 2u;
  return VoxelRasterFrameLoopResult{
      raster,
      elapsed_microseconds(loop->started),
      requested_frames,
      loop->stream_batches,
      raster.live_columns,
      raster.bounded_columns,
      raster.bounded_columns,
      raster.live_columns >= Radius32Columns,
      loop->session_create_microseconds,
      loop->stream_build_microseconds,
      loop->timing.stream_upload_microseconds,
      loop->timing.pass_graph_microseconds,
      loop->timing.draw_microseconds,
      loop->timing.submit_readback_microseconds,
      raster.retained_gpu_bytes,
      raster.upload_staging_bytes,
  };
}

void destroy_voxel_raster_world_frame_loop(VoxelRasterFrameLoopState *loop) {
  if (loop == nullptr) { return; }
  octaryn::client::rendering::destroy_slang_rhi_voxel_raster_frame_session(
      loop->session);
  delete loop;
}

VoxelRasterFrameLoopResult run_voxel_raster_world_frame_loop(
    std::uint32_t requested_frames) {
  VoxelRasterFrameLoopState *loop = create_voxel_raster_world_frame_loop();
  for (std::uint32_t frame = 0u; frame < requested_frames; ++frame) {
    if (!render_voxel_raster_world_frame(loop)) { break; }
  }
  VoxelRasterFrameLoopResult result =
      snapshot_voxel_raster_world_frame_loop(loop, requested_frames);
  destroy_voxel_raster_world_frame_loop(loop);
  return result;
}

} // namespace octaryn::client::world_presentation
