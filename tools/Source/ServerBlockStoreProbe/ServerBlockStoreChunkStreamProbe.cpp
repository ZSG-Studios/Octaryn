#include "BlockStore.h"
#include "ChunkColumnStream.h"

#include <cstdint>
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
  ok &= expect_equal(
      "chunk stream optional events fill",
      octaryn_server_chunk_stream_fill(
          &store, 0, 0, 1u, 1u, 0, 0, 1u, 1u, nullptr, 0u, columns_only.data(),
          static_cast<uint32_t>(columns_only.size()), no_blocks.data(),
          static_cast<uint32_t>(no_blocks.size()), &written),
      0);
  ok &= expect_equal("optional events still reports events",
                     written.event_count, 9u);
  return ok;
}

bool validate_chunk_request_frame() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 5));
  store.set_block(edit(32, 1, 0, 6));

  std::vector<octaryn_chunk_column_snapshot_column> columns(9u);
  std::vector<octaryn_chunk_column_snapshot_block> blocks(2u);
  octaryn_chunk_column_request_frame request{};
  request.version = 1u;
  request.size = OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE;
  request.center_chunk_x = 0;
  request.center_chunk_z = 0;
  request.radius = 1u;
  request.column_capacity = static_cast<uint32_t>(columns.size());
  request.block_capacity = static_cast<uint32_t>(blocks.size());
  request.columns_address = reinterpret_cast<uint64_t>(columns.data());
  request.blocks_address = reinterpret_cast<uint64_t>(blocks.data());

  bool ok = true;
  ok &= expect_equal(
      "chunk request result",
      octaryn_server_chunk_stream_request_columns(&store, &request), 0);
  ok &= expect_equal("chunk request status", request.status, 0u);
  ok &= expect_equal("chunk request columns", request.column_count, 9u);
  ok &= expect_equal("chunk request blocks", request.block_count, 2u);
  ok &= expect_equal("chunk request first column version", columns[0].version,
                     1u);
  ok &= expect_equal("chunk request first column x", columns[0].chunk_x, -1);
  ok &= expect_equal("chunk request center column block count",
                     columns[4].block_count, 1u);
  ok &=
      expect_equal("chunk request first block version", blocks[0].version, 1u);
  ok &= expect_equal("chunk request east block", blocks[1].block, uint16_t{6});

  request.column_capacity = 1u;
  request.block_capacity = static_cast<uint32_t>(blocks.size());
  ok &= expect_equal(
      "chunk request column capacity result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -3);
  ok &=
      expect_equal("chunk request required columns", request.column_count, 9u);
  ok &= expect_equal("chunk request capacity status", request.status, 3u);

  request.column_capacity = static_cast<uint32_t>(columns.size());
  request.block_capacity = 1u;
  ok &= expect_equal(
      "chunk request block capacity result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -4);
  ok &= expect_equal("chunk request required blocks", request.block_count, 2u);
  ok &= expect_equal("chunk request block capacity status", request.status,
                     4u);

  request.radius = 33u;
  request.block_capacity = static_cast<uint32_t>(blocks.size());
  request.column_capacity = static_cast<uint32_t>(columns.size());
  ok &= expect_equal(
      "chunk request radius result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -2);
  ok &= expect_equal("chunk request radius status", request.status, 2u);

  ok &= expect_equal(
      "chunk request unavailable result",
      octaryn_server_chunk_stream_write_request_result(&request, 0u, 0u, 5u),
      -5);
  ok &= expect_equal("chunk request unavailable status", request.status, 5u);

  octaryn_chunk_column_request_frame invalid_request{};
  ok &= expect_equal(
      "chunk request invalid result writer",
      octaryn_server_chunk_stream_write_request_result(&invalid_request, 0u,
                                                       0u, 5u),
      -1);
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
  ok &= expect_equal("snapshot file column count", result.counts.column_count,
                     9u);
  ok &=
      expect_equal("snapshot file block count", result.counts.block_count, 2u);

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

bool validate_chunk_stream_write_tracker() {
  void *tracker = octaryn_server_chunk_stream_write_tracker_create();
  bool ok = true;

  auto first = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 1u, 0u, 0, 0, 4u, 0u, 0, 0, 0u);
  ok &= expect_equal("first metadata write uses previous",
                     first.use_previous_window, 0u);
  ok &= expect_equal("first metadata write allowed", first.should_write, 1u);

  octaryn_server_chunk_stream_write_tracker_note_written(tracker, 0, 0, 4u);
  auto repeated = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 1u, 0u, 0, 0, 4u, 1u, 0, 0, 4u);
  ok &= expect_equal("repeated metadata uses previous",
                     repeated.use_previous_window, 1u);
  ok &= expect_equal("repeated metadata skips", repeated.should_write, 0u);

  auto changed = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 1u, 0u, 1, 0, 4u, 1u, 0, 0, 4u);
  ok &= expect_equal("changed metadata trusts previous",
                     changed.use_previous_window, 1u);
  ok &= expect_equal("changed metadata writes", changed.should_write, 1u);

  auto submitted = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 1u, 1u, 0, 0, 4u, 1u, 0, 0, 4u);
  ok &= expect_equal("submitted commands force write", submitted.should_write,
                     1u);

  auto full = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 0u, 0u, 0, 0, 4u, 1u, 0, 0, 4u);
  ok &= expect_equal("full stream uses caller previous",
                     full.use_previous_window, 1u);
  ok &= expect_equal("full stream writes", full.should_write, 1u);

  octaryn_server_chunk_view_intent intent{};
  intent.center_chunk_x = 0;
  intent.center_chunk_z = 0;
  intent.radius = 4u;
  intent.has_previous_window = 1u;
  intent.previous_center_chunk_x = 0;
  intent.previous_center_chunk_z = 0;
  intent.previous_radius = 4u;

  octaryn_server_chunk_stream_process_write_plan plan{};
  ok &= expect_equal(
      "process write plan result",
      octaryn_server_chunk_stream_plan_process_write(
          tracker, 0, 1u, &intent, 1u, 0u, &plan),
      0);
  ok &= expect_equal("process write plan continues", plan.should_continue, 1u);
  ok &= expect_equal("process write plan skips unchanged", plan.should_write,
                     0u);
  ok &= expect_equal("process write plan unchanged reason", plan.reason, 6u);

  intent.center_chunk_x = 2;
  ok &= expect_equal(
      "process write changed plan result",
      octaryn_server_chunk_stream_plan_process_write(
          tracker, 0, 1u, &intent, 1u, 0u, &plan),
      0);
  ok &= expect_equal("process write changed writes", plan.should_write, 1u);
  ok &= expect_equal("process write changed center", plan.center_chunk_x, 2);
  octaryn_server_chunk_stream_process_write_plan_note_written(tracker, &plan);
  auto changed_repeat = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, 1u, 0u, 2, 0, 4u, 1u, 2, 0, 4u);
  ok &= expect_equal("process note written skips repeat",
                     changed_repeat.should_write, 0u);

  ok &= expect_equal(
      "process write transient result",
      octaryn_server_chunk_stream_plan_process_write(
          tracker, -2, 1u, &intent, 1u, 0u, &plan),
      0);
  ok &= expect_equal("process write transient stops", plan.should_continue,
                     0u);
  ok &= expect_equal("process write transient handle", plan.handle_result, 0);
  ok &= expect_equal("process write transient reason", plan.reason, 2u);

  const auto tick = octaryn_server_chunk_stream_decide_process_tick(0u, 1u, 1u);
  ok &= expect_equal("process tick submitted command", tick.should_tick, 1u);
  ok &= expect_equal("process tick default frame", tick.use_default_frame, 1u);
  ok &= expect_equal("process tick host only", tick.use_host_only_tick, 1u);

  octaryn_host_frame_snapshot frame{};
  ok &= expect_equal("process frame create",
                     octaryn_server_chunk_stream_create_process_frame(&frame),
                     0);
  ok &= expect_equal("process frame version", frame.version, 1u);
  ok &= expect_equal("process frame size", frame.size,
                     OCTARYN_HOST_FRAME_SNAPSHOT_SIZE);
  ok &= expect_equal("process frame index", frame.timing.frame_index,
                     uint64_t{1u});

  octaryn_server_chunk_stream_write_tracker_destroy(tracker);
  return ok;
}

bool validate_chunk_view_intent_file() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_block_store_probe_chunk_view_intent.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  output << "{\n"
         << "  \"version\": 1,\n"
         << "  \"epoch\": 11,\n"
         << "  \"centerChunkX\": 3,\n"
         << "  \"centerChunkZ\": -2,\n"
         << "  \"radius\": 4,\n"
         << "  \"hasPreviousWindow\": true,\n"
         << "  \"previousCenterChunkX\": 1,\n"
         << "  \"previousCenterChunkZ\": -1,\n"
         << "  \"previousRadius\": 2\n"
         << "}\n";
  output.close();

  const std::string output_path_text = output_path.string();
  octaryn_server_chunk_view_intent intent{};
  bool ok = true;
  ok &= expect_equal("chunk view intent read",
                     octaryn_server_chunk_stream_read_view_intent(
                         output_path_text.c_str(), &intent),
                     0);
  ok &= expect_equal("chunk view intent epoch", intent.epoch, uint64_t{11u});
  ok &= expect_equal("chunk view intent center x", intent.center_chunk_x, 3);
  ok &= expect_equal("chunk view intent center z", intent.center_chunk_z, -2);
  ok &= expect_equal("chunk view intent radius", intent.radius, 4u);
  ok &= expect_equal("chunk view intent has previous",
                     intent.has_previous_window, 1u);
  ok &= expect_equal("chunk view intent previous radius",
                     intent.previous_radius, 2u);

  output.open(output_path, std::ios::binary | std::ios::trunc);
  output << "{\"version\":1,\"radius\":33}\n";
  output.close();
  ok &= expect_equal("chunk view intent rejects radius",
                     octaryn_server_chunk_stream_read_view_intent(
                         output_path_text.c_str(), &intent),
                     -4);

  std::filesystem::remove(output_path, error);
  return ok;
}

bool validate_block_interaction_intent_file() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_block_store_probe_block_interaction_intent.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  output << "{\"version\":1,\"frameIndex\":9,\"commands\":[{"
            "\"requestId\":44,\"editX\":1,\"editY\":2,\"editZ\":3,"
            "\"block\":7,\"cameraX\":0.5,\"cameraY\":1.5,"
            "\"cameraZ\":2.5,\"hitX\":4,\"hitY\":5,\"hitZ\":6},{"
            "\"requestId\":45,\"editX\":4,\"editY\":5,\"editZ\":6,"
            "\"block\":0,\"cameraX\":1.5,\"cameraY\":2.5,"
            "\"cameraZ\":3.5,\"hitX\":4,\"hitY\":5,\"hitZ\":6}]}\n";
  output.close();

  const std::string output_path_text = output_path.string();
  octaryn_host_command commands[2]{};
  octaryn_server_block_interaction_intent_result result{};
  bool ok = true;
  ok &= expect_equal(
      "block interaction intent read",
      octaryn_server_block_interaction_read_intent_file(
          output_path_text.c_str(), commands, 2u, &result),
      0);
  ok &= expect_equal("block interaction intent frame", result.frame_index,
                     uint64_t{9u});
  ok &= expect_equal("block interaction intent count", result.command_count,
                     2u);
  ok &= expect_equal("block interaction intent break count",
                     result.break_command_count, 1u);
  ok &= expect_equal("block interaction intent place count",
                     result.place_command_count, 1u);
  ok &= expect_equal("block interaction command kind", commands[0].kind, 1u);
  ok &= expect_equal("block interaction command request",
                     commands[0].request_id, uint64_t{44u});
  ok &= expect_equal("block interaction command edit x", commands[0].a, 1);
  ok &= expect_equal("block interaction command block", commands[0].d, 7);
  ok &= expect_equal("block interaction command hit z",
                     static_cast<int32_t>(commands[0].z2), 6);
  ok &= expect_equal("block interaction break command block", commands[1].d, 0);

  void *tracker = octaryn_server_block_interaction_frame_tracker_create();
  auto decision =
      octaryn_server_block_interaction_frame_tracker_decide(tracker,
                                                            result.frame_index);
  ok &= expect_equal("block interaction frame first submit",
                     decision.should_submit, 1u);
  ok &= expect_equal("block interaction frame first duplicate",
                     decision.duplicate_frame, 0u);
  octaryn_server_block_interaction_frame_tracker_note_submitted(
      tracker, result.frame_index);
  decision = octaryn_server_block_interaction_frame_tracker_decide(
      tracker, result.frame_index);
  ok &= expect_equal("block interaction frame duplicate submit",
                     decision.should_submit, 0u);
  ok &= expect_equal("block interaction frame duplicate flag",
                     decision.duplicate_frame, 1u);
  decision = octaryn_server_block_interaction_frame_tracker_decide(
      tracker, result.frame_index + 1u);
  ok &= expect_equal("block interaction frame next submit",
                     decision.should_submit, 1u);
  octaryn_server_block_interaction_frame_tracker_destroy(tracker);

  output.open(output_path, std::ios::binary | std::ios::trunc);
  output << "{\"version\":1,\"frameIndex\":9,\"commands\":[{\"requestId\":0}]}\n";
  output.close();
  ok &= expect_equal(
      "block interaction intent rejects unsupported",
      octaryn_server_block_interaction_read_intent_file(
          output_path_text.c_str(), commands, 2u, &result),
      -4);

  std::filesystem::remove(output_path, error);
  return ok;
}

} // namespace

bool validate_chunk_stream() {
  bool ok = true;
  ok &= validate_chunk_stream_arrays();
  ok &= validate_chunk_request_frame();
  ok &= validate_chunk_stream_snapshot_file();
  ok &= validate_chunk_stream_write_tracker();
  ok &= validate_chunk_view_intent_file();
  ok &= validate_block_interaction_intent_file();
  return ok;
}
