#include "VoxelRasterFrameLoop.h"

#include "BlockStore.h"
#include "ChunkColumnStream.h"
#include "../Environment/ProcessEnvironment.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

constexpr std::uint32_t ProbeFrames = 3u;
constexpr std::uint32_t Radius32Columns = 4225u;

bool write_server_authored_chunk_stream(const std::filesystem::path &path,
                                        std::uint32_t radius) {
  namespace blocks = octaryn::server::world::blocks;
  blocks::BlockStore store;
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{-32, 0, -32}, static_cast<std::uint16_t>(51)});
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{0, 1, -32}, static_cast<std::uint16_t>(52)});
  store.set_block(blocks::BlockEdit{
      blocks::BlockPosition{32, 2, -32}, static_cast<std::uint16_t>(53)});

  const std::string path_text = path.string();
  std::error_code error;
  std::filesystem::remove(path, error);
  std::filesystem::remove(path_text + ".bin", error);

  octaryn_server_chunk_stream_snapshot_request request{};
  request.stream_path = path_text.c_str();
  request.epoch = 17u;
  request.center_chunk_x = 0;
  request.center_chunk_z = 0;
  request.radius = radius;
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

bool prepare_server_authored_chunk_stream() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_client_voxel_world_frame_loop_probe_chunk_stream.json";
  return write_server_authored_chunk_stream(path, 32u);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 3 &&
      std::string{argv[1]} == "--write-radius32-chunk-stream") {
    if (!write_server_authored_chunk_stream(std::filesystem::path{argv[2]},
                                            32u)) {
      std::fprintf(stderr,
                   "failed to write radius-32 chunk stream fixture path=%s\n",
                   argv[2]);
      return 1;
    }
    std::printf("client_radius32_chunk_stream_fixture=passed path=%s\n",
                argv[2]);
    return 0;
  }
  if (argc != 1) {
    std::fprintf(stderr,
                 "usage: %s [--write-radius32-chunk-stream <path>]\n",
                 argv[0]);
    return 2;
  }
  if (!prepare_server_authored_chunk_stream()) {
    std::fprintf(stderr, "failed to prepare server-authored chunk stream\n");
    return 1;
  }
  auto *loop =
      octaryn::client::world_presentation::
          create_voxel_raster_world_frame_loop();
  for (std::uint32_t frame = 0u; frame < ProbeFrames; ++frame) {
    if (!octaryn::client::world_presentation::render_voxel_raster_world_frame(
            loop)) {
      break;
    }
  }
  const auto result =
      octaryn::client::world_presentation::
          snapshot_voxel_raster_world_frame_loop(loop, ProbeFrames);
  octaryn::client::world_presentation::destroy_voxel_raster_world_frame_loop(
      loop);

  const auto &raster = result.raster;
  if (!raster.color_readback_valid || raster.frames_rendered != ProbeFrames ||
      !raster.session_reused || !raster.live_stream_source ||
      !raster.indirect_draw_encoded || result.stream_batches != ProbeFrames ||
      raster.live_columns != Radius32Columns || !result.radius32_stream_available ||
      result.retained_gpu_bytes == 0u || result.upload_staging_bytes == 0u ||
      result.pass_graph_microseconds == 0u || result.draw_microseconds == 0u ||
      result.submit_readback_microseconds == 0u) {
    std::fprintf(stderr,
                 "client voxel world frame loop probe failed: status=%s "
                 "frames=%u batches=%u columns=%u live_columns=%u source=%s "
                 "reused=%d indirect=%d readback=%d radius32=%d "
                 "retained_gpu_bytes=%llu upload_staging_bytes=%llu "
                 "pass_graph_us=%llu draw_us=%llu "
                 "submit_readback_us=%llu\n",
                 raster.status, raster.frames_rendered, result.stream_batches,
                 raster.bounded_columns, raster.live_columns,
                 raster.live_stream_source ? "live_sidecar" : "fallback_fixture",
                 raster.session_reused ? 1 : 0,
                 raster.indirect_draw_encoded ? 1 : 0,
                 raster.color_readback_valid ? 1 : 0,
                 result.radius32_stream_available ? 1 : 0,
                 static_cast<unsigned long long>(result.retained_gpu_bytes),
                 static_cast<unsigned long long>(result.upload_staging_bytes),
                 static_cast<unsigned long long>(
                     result.pass_graph_microseconds),
                 static_cast<unsigned long long>(result.draw_microseconds),
                 static_cast<unsigned long long>(
                     result.submit_readback_microseconds));
    return 1;
  }

  std::printf("client_voxel_world_frame_loop_probe=passed status=%s "
              "requested_frames=%u frames=%u batches=%u columns=%u "
              "live_columns=%u retained_chunks=%u radius32_stream_available=%d "
              "source=%s reused=%d updates=%u draws=%u instances=%u "
              "upload_us=%llu retained_gpu_bytes=%llu "
              "upload_staging_bytes=%llu "
              "pass_graph_us=%llu draw_us=%llu submit_readback_us=%llu\n",
              raster.status, result.requested_frames, raster.frames_rendered,
              result.stream_batches, raster.bounded_columns,
              raster.live_columns, result.retained_chunks,
              result.radius32_stream_available ? 1 : 0,
              raster.live_stream_source ? "live_sidecar" : "fallback_fixture",
              raster.session_reused ? 1 : 0, raster.stream_updates,
              raster.draw_count, raster.total_instances,
              static_cast<unsigned long long>(result.upload_microseconds),
              static_cast<unsigned long long>(result.retained_gpu_bytes),
              static_cast<unsigned long long>(result.upload_staging_bytes),
              static_cast<unsigned long long>(result.pass_graph_microseconds),
              static_cast<unsigned long long>(result.draw_microseconds),
              static_cast<unsigned long long>(
                  result.submit_readback_microseconds));
  return 0;
}
