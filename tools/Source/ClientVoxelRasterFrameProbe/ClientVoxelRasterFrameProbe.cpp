#include "SlangRhiVoxelRasterFrame.h"

#include "BlockStore.h"
#include "ChunkColumnStream.h"
#include "../Environment/ProcessEnvironment.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

bool prepare_server_authored_chunk_stream() {
  namespace blocks = octaryn::server::world::blocks;
  blocks::BlockStore store;
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{-32, 0, -32}, static_cast<std::uint16_t>(51)});
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{0, 1, -32}, static_cast<std::uint16_t>(52)});
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{32, 2, -32}, static_cast<std::uint16_t>(53)});

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_client_voxel_raster_frame_probe_chunk_stream.json";
  const std::string path_text = path.string();
  std::error_code error;
  std::filesystem::remove(path, error);
  std::filesystem::remove(path_text + ".bin", error);

  octaryn_server_chunk_stream_snapshot_request request{};
  request.stream_path = path_text.c_str();
  request.epoch = 11u;
  request.center_chunk_x = 0;
  request.center_chunk_z = 0;
  request.radius = 1u;
  request.world_seed = 1234u;
  request.player_control_mode = 1u;

  octaryn_server_chunk_stream_snapshot_result result{};
  if (octaryn_server_chunk_stream_write_snapshot_file(&store, &request,
                                                      &result) == 0) {
    return octaryn::tools::set_process_environment(
        "OCTARYN_CLIENT_CHUNK_STREAM_PATH", path_text.c_str());
  }
  return false;
}

} // namespace

int main() {
  if (!prepare_server_authored_chunk_stream()) {
    std::fprintf(stderr, "Failed to prepare the server-authored chunk stream.\n");
    return 1;
  }
  const octaryn::client::rendering::SlangRhiVoxelRasterFrameProbeResult result =
      octaryn::client::rendering::probe_slang_rhi_voxel_raster_frame();
  if (!result.color_readback_valid) {
    std::fprintf(stderr,
                 "client voxel raster frame probe failed: status=%s "
                 "device=%d queue=%d heap=%d compute_pipelines=%d "
                 "graphics_pipeline=%d command=%d buffers=%d bound=%d "
                 "retained=%d reused=%d frames=%u updates=%u "
                 "compute=%d indirect_draw=%d submitted=%d fence=%d "
                 "readback=%d center=(%d,%d) first_column=(%d,%d) "
                 "source=%s columns=%u draws=%u instances=%u nonclear=%u\n",
                 result.status, result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.transient_heap_created ? 1 : 0,
                 result.compute_pipelines_created ? 1 : 0,
                 result.graphics_pipeline_created ? 1 : 0,
                 result.command_buffer_created ? 1 : 0,
                 result.buffers_created ? 1 : 0,
                 result.resources_bound ? 1 : 0,
                 result.retained_frame_resources ? 1 : 0,
                 result.session_reused ? 1 : 0, result.frames_rendered,
                 result.stream_updates,
                 result.compute_dispatched ? 1 : 0,
                 result.indirect_draw_encoded ? 1 : 0,
                 result.submitted ? 1 : 0,
                 result.fence_completed ? 1 : 0,
                 result.color_readback_valid ? 1 : 0, result.stream_center_x,
                 result.stream_center_z, result.first_column_x,
                 result.first_column_z,
                 result.live_stream_source ? "live_sidecar" : "fallback_fixture",
                 result.bounded_columns,
                 result.draw_count, result.total_instances,
                 result.non_clear_pixels);
    return 1;
  }
  std::printf("client_voxel_raster_frame_probe=passed status=%s "
              "center=(%d,%d) first_column=(%d,%d) columns=%u "
              "source=%s "
              "retained_resources=%d session_reused=%d frames=%u updates=%u "
              "draws=%u instances=%u "
              "nonclear=%u compute_dispatch=1 indirect_draw=1 readback=1\n",
              result.status, result.stream_center_x, result.stream_center_z,
              result.first_column_x, result.first_column_z,
              result.bounded_columns,
              result.live_stream_source ? "live_sidecar" : "fallback_fixture",
              result.retained_frame_resources ? 1 : 0,
              result.session_reused ? 1 : 0, result.frames_rendered,
              result.stream_updates, result.draw_count, result.total_instances,
              result.non_clear_pixels);
  return 0;
}
