#include "PresentationSnapshots.h"

#include "EmptyWorldMesh.h"
#include "Log.h"
#include "FunctionProfile.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <utility>

namespace octaryn_client_app {

namespace {

constexpr uint64_t kSteadyServerStreamPollFrames = 4u;

void add_dirty_column(std::vector<empty_world_dirty_column> &columns,
                      int32_t chunk_x, int32_t chunk_z) {
  for (const empty_world_dirty_column &column : columns) {
    if (column.chunk_x == chunk_x && column.chunk_z == chunk_z) {
      return;
    }
  }
  columns.push_back(empty_world_dirty_column{chunk_x, chunk_z});
}

void add_dirty_columns_for_block(std::vector<empty_world_dirty_column> &columns,
                                 const block_position_key &key) {
  constexpr int32_t kChunkWidth = 32;
  const int32_t chunk_x = key.x >= 0 ? key.x / kChunkWidth
                                    : (key.x - kChunkWidth + 1) / kChunkWidth;
  const int32_t chunk_z = key.z >= 0 ? key.z / kChunkWidth
                                    : (key.z - kChunkWidth + 1) / kChunkWidth;
  const int32_t local_x = key.x - chunk_x * kChunkWidth;
  const int32_t local_z = key.z - chunk_z * kChunkWidth;

  add_dirty_column(columns, chunk_x, chunk_z);
  if (local_x == 0) {
    add_dirty_column(columns, chunk_x - 1, chunk_z);
  } else if (local_x == kChunkWidth - 1) {
    add_dirty_column(columns, chunk_x + 1, chunk_z);
  }
  if (local_z == 0) {
    add_dirty_column(columns, chunk_x, chunk_z - 1);
  } else if (local_z == kChunkWidth - 1) {
    add_dirty_column(columns, chunk_x, chunk_z + 1);
  }
}

std::vector<empty_world_dirty_column> collect_dirty_override_columns(
    const block_lookup &previous_overrides,
    const std::vector<world_block_record> &next_overrides) {
  std::vector<empty_world_dirty_column> columns;
  for (const auto &entry : previous_overrides) {
    add_dirty_columns_for_block(columns, entry.first);
  }
  for (const world_block_record &record : next_overrides) {
    add_dirty_columns_for_block(
        columns, block_position_key{record.x, record.y, record.z});
  }
  return columns;
}

} // namespace

void place_camera_over_snapshot(camera &camera,
                                const std::vector<presentation_block> &blocks) {
  if (blocks.empty()) {
    return;
  }

  int32_t min_x = blocks.front().x;
  int32_t max_x = blocks.front().x;
  int32_t min_y = blocks.front().y;
  int32_t max_y = blocks.front().y;
  int32_t min_z = blocks.front().z;
  int32_t max_z = blocks.front().z;
  for (const presentation_block &block : blocks) {
    min_x = std::min(min_x, block.x);
    max_x = std::max(max_x, block.x);
    min_y = std::min(min_y, block.y);
    max_y = std::max(max_y, block.y);
    min_z = std::min(min_z, block.z);
    max_z = std::max(max_z, block.z);
  }

  camera.position[0] =
      (static_cast<float>(min_x) + static_cast<float>(max_x)) * 0.5f;
  camera.position[1] = static_cast<float>(min_y) + 2.0f;
  camera.position[2] =
      (static_cast<float>(min_z) + static_cast<float>(max_z)) * 0.5f;
  camera_update(&camera);

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "snapshot_camera_origin x=%.3f y=%.3f z=%.3f "
                 "bounds=(%" PRId32 ",%" PRId32 ")-"
                 "(%" PRId32 ",%" PRId32 ")-"
                 "(%" PRId32 ",%" PRId32 ")\n",
                 camera.position[0], camera.position[1], camera.position[2],
                 min_x, max_x, min_y, max_y, min_z, max_z);
    std::fflush(g_log);
  }
}

bool poll_server_stream_presentation(
    const singleplayer_server_session &server_session,
    bool game_modules_disabled,
    const chunk_view &empty_world_mesh_chunk_view,
    uint64_t frame_index, client_server_stream_poll_state &poll_state,
    server_world_time_state &world_time,
    std::vector<presentation_block> &world_snapshot_blocks,
    std::vector<presentation_block> &world_surface_blocks,
    block_lookup &world_block_lookup, camera &camera,
    bool &empty_world_stream_mesh_dirty, int &result) {
  empty_world_stream_mesh_dirty = false;
  if (!server_session.enabled) {
    return true;
  }

  function_profile_scope profile_scope("server_stream_poll",
                                                      frame_index, "");
  if (!poll_state.loaded_server_world_blocks &&
      server_session.chunk_stream_path.empty() &&
      std::filesystem::exists(server_session.world_blocks_path)) {
    if (load_world_blocks_from_path(server_session.world_blocks_path,
                                    world_snapshot_blocks,
                                    world_surface_blocks)) {
      poll_state.loaded_server_world_blocks = true;
      world_block_lookup = build_block_lookup(world_snapshot_blocks);
      place_camera_over_snapshot(camera, world_surface_blocks);
      camera_update(&camera);
      result = apply_snapshot_blocks(world_snapshot_blocks, frame_index + 9u);
      log_result("server_world_blocks_snapshot", result);
      if (result != 0) {
        return false;
      }
    }
  }

  if (!poll_state.active_server_stream.columns.empty() &&
      frame_index < poll_state.next_server_stream_poll_frame) {
    return true;
  }

  std::error_code stream_time_error;
  const auto stream_write_time = std::filesystem::last_write_time(
      server_session.chunk_stream_path, stream_time_error);
  if (stream_time_error ||
      stream_write_time == poll_state.active_server_stream_write_time) {
    return true;
  }

  server_chunk_stream_file loaded_stream{};
  if (!load_server_chunk_stream_file(loaded_stream, world_time, true)) {
    result = -9;
    return false;
  }

  poll_state.active_server_stream_write_time = stream_write_time;
  const chunk_view loaded_stream_view =
      chunk_view_from_server_stream(loaded_stream);
  const bool stream_view_changed =
      !same_chunk_view(empty_world_mesh_chunk_view, loaded_stream_view);
  const uint64_t loaded_override_signature =
      hash_world_block_records(loaded_stream.blocks);
  const bool override_records_changed =
      loaded_override_signature !=
      poll_state.active_server_stream_override_signature;
  poll_state.active_server_stream_dirty_columns.clear();
  if (override_records_changed) {
    poll_state.active_server_stream_dirty_columns =
        collect_dirty_override_columns(world_block_lookup,
                                       loaded_stream.blocks);
    apply_empty_world_overrides_from_records(loaded_stream.blocks,
                                             world_block_lookup);
    poll_state.active_server_stream_override_signature =
        loaded_override_signature;
  }
  empty_world_stream_mesh_dirty =
      stream_view_changed || override_records_changed;
  poll_state.next_server_stream_poll_frame =
      frame_index + (empty_world_stream_mesh_dirty
                         ? 1u
                         : kSteadyServerStreamPollFrames);

  if (game_modules_disabled) {
    poll_state.active_server_stream = std::move(loaded_stream);
    if (!empty_world_stream_mesh_dirty && g_log != nullptr) {
      std::fprintf(
          g_log,
          "native_empty_chunk_stream active=1 source=server_background "
          "rebuild=0 reason=time_only_stream epoch=%" PRIu64
          " render_distance=%" PRIu32 " columns=%zu override_edits=%zu "
          "world_time_day_fraction=%.6f\n",
          poll_state.active_server_stream.epoch,
          poll_state.active_server_stream.radius,
          poll_state.active_server_stream.columns.size(),
          world_block_lookup.size(), world_time.day_fraction);
      std::fflush(g_log);
    }
    return true;
  }

  if (!loaded_stream.blocks.empty()) {
    poll_state.active_server_stream = std::move(loaded_stream);
    apply_blocks_from_records(poll_state.active_server_stream.blocks, false,
                              world_snapshot_blocks);
    apply_top_blocks_from_records(poll_state.active_server_stream.blocks, false,
                                  world_surface_blocks);
    if (!world_snapshot_blocks.empty()) {
      result = apply_snapshot_blocks(world_snapshot_blocks, frame_index + 10u);
      log_result("server_chunk_stream_snapshot", result);
      if (result != 0) {
        return false;
      }
    }
  } else {
    poll_state.active_server_stream = std::move(loaded_stream);
  }

  return true;
}

} // namespace octaryn_client_app
