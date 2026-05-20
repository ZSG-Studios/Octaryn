#include "PresentationSnapshots.h"

#include "EmptyWorldMesh.h"
#include "FunctionProfile.h"
#include "Log.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <utility>

namespace octaryn_client_app {

namespace {

constexpr uint64_t kSteadyServerStreamPollFrames = 4u;
constexpr float kWalkAuthorityCorrectionAlpha = 0.2f;
constexpr float kWalkAuthoritySnapDistance = 8.0f;
constexpr float kWalkAuthorityVerticalSnapDistance = 4.0f;

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

void reconcile_player_camera(float player_x, float player_y, float player_z,
                             float velocity_x, float velocity_y,
                             float velocity_z, uint32_t control_mode,
                             uint32_t on_ground, const char *source,
                             camera &camera, uint64_t frame_index,
                             bool force_snap) {
  constexpr uint32_t kWalkControlMode = 0u;
  const float dx = player_x - camera.position[0];
  const float dy = player_y - camera.position[1];
  const float dz = player_z - camera.position[2];
  const float horizontal_error = std::sqrt(dx * dx + dz * dz);
  const float vertical_error = std::fabs(dy);
  const bool server_position_authoritative = control_mode == kWalkControlMode;
  const bool snap = force_snap ||
                    horizontal_error > kWalkAuthoritySnapDistance ||
                    vertical_error > kWalkAuthorityVerticalSnapDistance ||
                    !std::isfinite(horizontal_error);
  if (snap) {
    camera.position[0] = player_x;
    camera.position[1] = player_y;
    camera.position[2] = player_z;
    camera_update(&camera);
  } else if (server_position_authoritative) {
    camera.position[0] += dx * kWalkAuthorityCorrectionAlpha;
    camera.position[1] += dy * kWalkAuthorityCorrectionAlpha;
    camera.position[2] += dz * kWalkAuthorityCorrectionAlpha;
    camera_update(&camera);
  }
  if (g_log != nullptr && frame_index % 30u == 0u) {
    std::fprintf(g_log,
                 "live_player_authority source=%s "
                 "position=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f) "
                 "mode=%" PRIu32 " on_ground=%" PRIu32
                 " correction=(%.3f,%.3f,%.3f) horizontal_error=%.3f "
                 "snap=%d\n",
                 source, player_x, player_y, player_z, velocity_x, velocity_y,
                 velocity_z, control_mode, on_ground, dx, dy, dz,
                 horizontal_error, snap ? 1 : 0);
    std::fflush(g_log);
  }
}

void reconcile_stream_player_camera(const server_chunk_stream_file &stream,
                                    camera &camera, uint64_t frame_index,
                                    bool force_snap) {
  reconcile_player_camera(
      stream.playerX, stream.playerY, stream.playerZ, stream.playerVelocityX,
      stream.playerVelocityY, stream.playerVelocityZ, stream.playerControlMode,
      stream.playerOnGround, "server_stream", camera, frame_index, force_snap);
}

void reconcile_player_state_camera(const server_player_state_stream_file &state,
                                   camera &camera, uint64_t frame_index,
                                   bool force_snap) {
  reconcile_player_camera(
      state.playerX, state.playerY, state.playerZ, state.playerVelocityX,
      state.playerVelocityY, state.playerVelocityZ, state.playerControlMode,
      state.playerOnGround, "server_player_state", camera, frame_index,
      force_snap);
}

bool poll_server_player_state_stream(
    const singleplayer_server_session &server_session, uint64_t frame_index,
    client_server_stream_poll_state &poll_state, camera &camera, int &result) {
  if (server_session.player_state_stream_path.empty()) {
    return true;
  }

  std::error_code state_time_error;
  const auto state_write_time = std::filesystem::last_write_time(
      server_session.player_state_stream_path, state_time_error);
  if (state_time_error ||
      state_write_time == poll_state.active_server_player_state_write_time) {
    return true;
  }

  server_player_state_stream_file state{};
  if (!load_server_player_state_stream_file(
          server_session.player_state_stream_path, state, true)) {
    result = -10;
    return false;
  }

  poll_state.active_server_player_state_write_time = state_write_time;
  reconcile_player_state_camera(state, camera, frame_index,
                                !poll_state.reconciled_initial_player_snapshot);
  poll_state.reconciled_initial_player_snapshot = true;
  return true;
}

bool next_override_contains(const std::vector<world_block_record> &records,
                            const block_position_key &key, uint16_t &block) {
  for (const world_block_record &record : records) {
    if (record.x == key.x && record.y == key.y && record.z == key.z) {
      block = record.block;
      return true;
    }
  }
  return false;
}

std::vector<empty_world_dirty_column> collect_dirty_override_columns(
    const block_lookup &previous_overrides,
    const std::vector<world_block_record> &next_overrides) {
  std::vector<empty_world_dirty_column> columns;
  for (const auto &entry : previous_overrides) {
    uint16_t next_block = 0u;
    if (!next_override_contains(next_overrides, entry.first, next_block) ||
        next_block != entry.second) {
      add_dirty_columns_for_block(columns, entry.first);
    }
  }
  for (const world_block_record &record : next_overrides) {
    const block_position_key key{record.x, record.y, record.z};
    uint16_t previous_block = 0u;
    if (!has_block_override(previous_overrides, key, previous_block) ||
        previous_block != record.block) {
      add_dirty_columns_for_block(columns, key);
    }
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
    bool game_modules_disabled, const chunk_view &empty_world_mesh_chunk_view,
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

  function_profile_scope profile_scope("server_stream_poll", frame_index, "");
  if (!poll_server_player_state_stream(server_session, frame_index, poll_state,
                                       camera, result)) {
    return false;
  }
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
  if (!poll_state.loaded_server_world_blocks) {
    poll_state.loaded_server_world_blocks = true;
    log_line("world_blocks_snapshot=0");
  }

  poll_state.active_server_stream_write_time = stream_write_time;
  const bool had_active_server_stream =
      !poll_state.active_server_stream.columns.empty();
  const chunk_view previous_stream_view =
      had_active_server_stream
          ? chunk_view_from_server_stream(poll_state.active_server_stream)
          : empty_world_mesh_chunk_view;
  const chunk_view loaded_stream_view =
      chunk_view_from_server_stream(loaded_stream);
  const bool stream_view_changed =
      !same_chunk_view(previous_stream_view, loaded_stream_view);
  const bool stream_has_authoritative_edits = !loaded_stream.blocks.empty();
  const uint64_t loaded_override_signature =
      stream_has_authoritative_edits ? hash_world_block_records(loaded_stream.blocks)
                                     : poll_state.active_server_stream_override_signature;
  const bool override_records_changed =
      stream_has_authoritative_edits &&
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
      frame_index +
      (empty_world_stream_mesh_dirty ? 1u : kSteadyServerStreamPollFrames);

  if (game_modules_disabled) {
    poll_state.active_server_stream = std::move(loaded_stream);
    reconcile_stream_player_camera(
        poll_state.active_server_stream, camera, frame_index,
        !poll_state.reconciled_initial_player_snapshot);
    poll_state.reconciled_initial_player_snapshot = true;
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
    reconcile_stream_player_camera(
        poll_state.active_server_stream, camera, frame_index,
        !poll_state.reconciled_initial_player_snapshot);
    poll_state.reconciled_initial_player_snapshot = true;
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
    reconcile_stream_player_camera(
        poll_state.active_server_stream, camera, frame_index,
        !poll_state.reconciled_initial_player_snapshot);
    poll_state.reconciled_initial_player_snapshot = true;
  }

  return true;
}

} // namespace octaryn_client_app
