#include "BlockStore.h"
#include "ChunkColumnStream.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace octaryn::server::world::blocks;

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_contains(std::string_view label, std::string_view text,
                     std::string_view expected) {
  if (text.find(expected) != std::string_view::npos) {
    return true;
  }

  std::fprintf(stderr, "%.*s: missing expected text\n",
               static_cast<int>(label.size()), label.data());
  return false;
}

BlockEdit edit(int32_t x, int32_t y, int32_t z, uint16_t block) {
  return BlockEdit{.position = BlockPosition{.x = x, .y = y, .z = z},
                   .block = block};
}

bool validate_chunk_stream_arrays() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 5));
  store.set_block(edit(32, 1, 0, 6));

  octaryn_server_chunk_stream_counts counts{};
  bool ok = true;
  ok &= expect_equal("chunk stream count result",
                     octaryn_server_chunk_stream_count(&store, 0, 0, 1u, 0u, 0,
                                                       0, 0u, 0u, &counts),
                     0);
  ok &= expect_equal("chunk stream events", counts.event_count, 9u);
  ok &= expect_equal("chunk stream columns", counts.column_count, 9u);
  ok &= expect_equal("chunk stream blocks", counts.block_count, 2u);

  std::vector<octaryn_server_chunk_window_event> events(counts.event_count);
  std::vector<octaryn_server_chunk_stream_column> columns(counts.column_count);
  std::vector<octaryn_server_chunk_stream_block> blocks(counts.block_count);
  octaryn_server_chunk_stream_counts written{};
  ok &= expect_equal("chunk stream fill result",
                     octaryn_server_chunk_stream_fill(
                         &store, 0, 0, 1u, 0u, 0, 0, 0u, 0u, events.data(),
                         static_cast<uint32_t>(events.size()), columns.data(),
                         static_cast<uint32_t>(columns.size()), blocks.data(),
                         static_cast<uint32_t>(blocks.size()), &written),
                     0);
  ok &= expect_equal("chunk stream written blocks", written.block_count, 2u);
  ok &= expect_equal("chunk stream first event load", events[0].kind, 0u);
  ok &= expect_equal("chunk stream first column x", columns[0].chunk_x, -1);
  ok &= expect_equal("chunk stream first column z", columns[0].chunk_z, -1);
  ok &= expect_equal("chunk stream center column block count",
                     columns[4].block_count, 1u);
  ok &= expect_equal("chunk stream center block", blocks[0].block, uint16_t{5});
  ok &= expect_equal("chunk stream east block", blocks[1].block, uint16_t{6});

  ok &= expect_equal("metadata count result",
                     octaryn_server_chunk_stream_count(&store, 0, 0, 1u, 1u, 0,
                                                       0, 1u, 1u, &counts),
                     0);
  ok &= expect_equal("metadata preserved block count", counts.block_count, 0u);
  ok &= expect_equal("metadata event count", counts.event_count, 9u);

  std::vector<octaryn_server_chunk_stream_column> columns_only(
      counts.column_count);
  std::vector<octaryn_server_chunk_stream_block> no_blocks(counts.block_count);
  ok &= expect_equal("chunk stream optional events fill",
                     octaryn_server_chunk_stream_fill(
                         &store, 0, 0, 1u, 1u, 0, 0, 1u, 1u, nullptr, 0u,
                         columns_only.data(),
                         static_cast<uint32_t>(columns_only.size()),
                         no_blocks.data(),
                         static_cast<uint32_t>(no_blocks.size()), &written),
                     0);
  ok &= expect_equal("optional events still reports events",
                     written.event_count, 9u);
  return ok;
}

bool validate_chunk_stream_snapshot_file() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 5));
  store.set_block(edit(32, 1, 0, 6));

  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_block_store_probe_chunk_stream.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);
  const std::string output_path_text = output_path.string();

  octaryn_server_chunk_stream_snapshot_request request{};
  request.stream_path = output_path_text.c_str();
  request.epoch = 7u;
  request.center_chunk_x = 0;
  request.center_chunk_z = 0;
  request.radius = 1u;
  request.world_time_day_index = 2u;
  request.world_time_second_of_day = 1200u;
  request.world_time_total_seconds = 1800.25;
  request.world_time_day_fraction = 0.25f;
  request.player_x = 1.0f;
  request.player_y = 2.0f;
  request.player_z = 3.0f;
  request.player_pitch = 4.0f;
  request.player_yaw = 5.0f;
  request.player_velocity_x = 0.1f;
  request.player_velocity_y = 0.2f;
  request.player_velocity_z = 0.3f;
  request.player_control_mode = 1u;
  request.player_on_ground = 1u;

  octaryn_server_chunk_stream_snapshot_result result{};
  bool ok = true;
  ok &= expect_equal("snapshot file write result",
                     octaryn_server_chunk_stream_write_snapshot_file(
                         &store, &request, &result),
                     0);
  ok &= expect_equal("snapshot file load count", result.load_count, 9u);
  ok &=
      expect_equal("snapshot file column count", result.counts.column_count, 9u);
  ok &= expect_equal("snapshot file block count", result.counts.block_count, 2u);

  std::ifstream input{output_path, std::ios::binary};
  const std::string text{std::istreambuf_iterator<char>{input},
                         std::istreambuf_iterator<char>{}};
  ok &= expect_contains("snapshot file source", text,
                        "\"source\": \"server_process_chunk_stream\"");
  ok &= expect_contains("snapshot file center", text, "\"centerChunkX\": 0");
  ok &= expect_contains("snapshot file player mode", text,
                        "\"playerControlMode\": \"fly\"");
  ok &= expect_contains("snapshot file load event", text, "\"kind\": \"load\"");
  ok &= expect_contains("snapshot file block payload", text, "\"block\": 6");

  std::filesystem::remove(output_path, error);
  return ok;
}

} // namespace

bool validate_chunk_stream() {
  bool ok = true;
  ok &= validate_chunk_stream_arrays();
  ok &= validate_chunk_stream_snapshot_file();
  return ok;
}
