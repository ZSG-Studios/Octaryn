#include "BlockStore.h"
#include "ChunkColumnStream.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

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

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
}

} // namespace

bool validate_chunk_stream_process_snapshot() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 5));
  store.set_block(edit(32, 1, 0, 6));

  void *tracker = octaryn_server_chunk_stream_write_tracker_create();
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_block_store_probe_process_stream.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);
  const std::string output_path_text = output_path.string();

  octaryn_server_chunk_view_intent intent{};
  intent.epoch = 17u;
  intent.center_chunk_x = 1;
  intent.center_chunk_z = 0;
  intent.radius = 1u;
  intent.has_previous_window = 1u;
  intent.previous_center_chunk_x = 0;
  intent.previous_center_chunk_z = 0;
  intent.previous_radius = 1u;

  octaryn_server_chunk_stream_process_write_plan plan{};
  bool ok = true;
  ok &= expect_equal("process snapshot plan",
                     octaryn_server_chunk_stream_plan_process_write(
                         tracker, 0, 0u, &intent, 1u, 0u, &plan),
                     0);
  ok &= expect_equal("process snapshot writes", plan.should_write, 1u);
  ok &= expect_equal("process snapshot uses requested center",
                     plan.center_chunk_x, 1);

  octaryn_server_chunk_stream_process_snapshot_request request{};
  request.stream_path = output_path_text.c_str();
  request.intent = intent;
  request.write_plan = plan;
  request.metadata_only = 1u;
  request.world_time_day_index = 4u;
  request.world_time_second_of_day = 500u;
  request.world_time_total_seconds = 500.25;
  request.world_time_day_fraction = 0.125f;
  request.player_x = 3.0f;
  request.player_y = 4.0f;
  request.player_z = 5.0f;
  request.player_control_mode = 1u;
  request.player_on_ground = 1u;

  octaryn_server_chunk_stream_snapshot_result result{};
  ok &= expect_equal("process snapshot write result",
                     octaryn_server_chunk_stream_write_process_snapshot_file(
                         &store, tracker, &request, &result),
                     0);
  ok &= expect_equal("process snapshot load count", result.load_count, 9u);
  ok &= expect_contains("process snapshot center", read_file(output_path),
                        "\"centerChunkX\": 1");

  octaryn_server_chunk_stream_process_write_plan repeat_plan{};
  ok &= expect_equal("process snapshot repeat plan",
                     octaryn_server_chunk_stream_plan_process_write(
                         tracker, 0, 0u, &intent, 1u, 0u, &repeat_plan),
                     0);
  ok &= expect_equal("process snapshot repeat skips",
                     repeat_plan.should_write, 0u);

  std::filesystem::remove(output_path, error);
  octaryn_server_chunk_stream_write_tracker_destroy(tracker);
  return ok;
}
