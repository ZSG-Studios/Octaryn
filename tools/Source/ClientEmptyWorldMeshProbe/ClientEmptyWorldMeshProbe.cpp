#include "EmptyWorldMesh.h"
#include "Log.h"
#include "Packing.h"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <unordered_set>

namespace octaryn_client_app {

FILE *g_log = nullptr;

bool has_block_override(const block_lookup &lookup,
                        const block_position_key &key, uint16_t &block) {
  const auto found = lookup.find(key);
  if (found == lookup.end()) {
    return false;
  }

  block = found->second;
  return true;
}

} // namespace octaryn_client_app

namespace {

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::server_chunk_stream_column_record;
using octaryn_client_app::server_chunk_stream_file;

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_equal(std::string_view label, size_t actual, size_t expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %zu, got %zu\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

server_chunk_stream_file make_single_column_stream() {
  server_chunk_stream_file stream{};
  stream.version = 1;
  stream.epoch = 1u;
  stream.source = "probe";
  stream.centerChunkX = 0;
  stream.centerChunkZ = 0;
  stream.radius = 0u;
  stream.columns.push_back(server_chunk_stream_column_record{.chunkX = 0,
                                                             .chunkZ = 0,
                                                             .originX = 0,
                                                             .originZ = 0,
                                                             .blockOffset = 0u,
                                                             .blockCount = 0u});
  return stream;
}

server_chunk_stream_file make_radius_stream(uint32_t radius) {
  server_chunk_stream_file stream{};
  stream.version = 1;
  stream.epoch = 2u;
  stream.source = "probe";
  stream.centerChunkX = 0;
  stream.centerChunkZ = 0;
  stream.radius = radius;
  for (int32_t chunk_z = -static_cast<int32_t>(radius);
       chunk_z <= static_cast<int32_t>(radius); ++chunk_z) {
    for (int32_t chunk_x = -static_cast<int32_t>(radius);
         chunk_x <= static_cast<int32_t>(radius); ++chunk_x) {
      stream.columns.push_back(server_chunk_stream_column_record{
          .chunkX = chunk_x,
          .chunkZ = chunk_z,
          .originX = chunk_x * kEmptyWorldChunkSize,
          .originZ = chunk_z * kEmptyWorldChunkSize,
          .blockOffset = 0u,
          .blockCount = 0u});
    }
  }
  return stream;
}
server_chunk_stream_file make_center_stream(int32_t center_x, int32_t center_z,
                                            uint32_t radius, uint64_t epoch) {
  server_chunk_stream_file stream = make_radius_stream(radius);
  stream.epoch = epoch;
  stream.centerChunkX = center_x;
  stream.centerChunkZ = center_z;
  for (server_chunk_stream_column_record &column : stream.columns) {
    column.chunkX += center_x;
    column.chunkZ += center_z;
    column.originX = column.chunkX * kEmptyWorldChunkSize;
    column.originZ = column.chunkZ * kEmptyWorldChunkSize;
  }
  return stream;
}
server_chunk_stream_file make_shifted_radius_stream() {
  server_chunk_stream_file stream{};
  stream.version = 1;
  stream.epoch = 3u;
  stream.source = "probe";
  stream.centerChunkX = 1;
  stream.centerChunkZ = 0;
  stream.radius = 1u;
  for (int32_t chunk_z = -1; chunk_z <= 1; ++chunk_z) {
    for (int32_t chunk_x = 0; chunk_x <= 2; ++chunk_x) {
      stream.columns.push_back(server_chunk_stream_column_record{
          .chunkX = chunk_x,
          .chunkZ = chunk_z,
          .originX = chunk_x * kEmptyWorldChunkSize,
          .originZ = chunk_z * kEmptyWorldChunkSize,
          .blockOffset = 0u,
          .blockCount = 0u});
    }
  }
  return stream;
}
world_mesh_upload_frame build_frame(const block_lookup &overrides) {
  world_mesh_upload_frame frame;
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  const server_chunk_stream_file stream = make_single_column_stream();
  build_empty_world_mesh_frame_from_stream_columns(
      stream, stream.columns, overrides, previous, dirty_columns, frame);
  return frame;
}

bool validate_batched_stream_mesh_matches_full_stream() {
  const server_chunk_stream_file stream = make_radius_stream(1u);
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  world_mesh_upload_frame full;
  build_empty_world_mesh_frame_from_stream_columns(
      stream, stream.columns, block_lookup{}, previous, dirty_columns, full);

  world_mesh_upload_frame batched;
  world_mesh_upload_frame retained;
  empty_world_stream_mesh_batch_result result;
  size_t cursor = 0u;
  size_t batches = 0u;
  do {
    world_mesh_upload_frame batch;
    build_empty_world_mesh_frame_from_stream_batch(stream, block_lookup{},
                                                   previous, dirty_columns,
                                                   retained, cursor, 2u, batch,
                                                   result);
    cursor = result.next_entry;
    ++batches;
    batched.chunks.insert(batched.chunks.end(), batch.chunks.begin(),
                          batch.chunks.end());
    batched.opaque_faces.insert(batched.opaque_faces.end(),
                                batch.opaque_faces.begin(),
                                batch.opaque_faces.end());
  } while (!result.complete);

  bool ok = true;
  ok &= expect_true("stream mesh uses multiple batches", batches > 1u);
  ok &= expect_equal("batched stream chunks", batched.chunks.size(),
                     full.chunks.size());
  ok &= expect_equal("batched stream opaque faces", batched.opaque_faces.size(),
                     full.opaque_faces.size());
  return ok;
}

bool validate_retained_stream_unloads_before_builds() {
  const server_chunk_stream_file stream = make_shifted_radius_stream();
  const chunk_view previous{-1, -1, 3};
  const std::vector<empty_world_dirty_column> dirty_columns;
  world_mesh_upload_frame retained;
  for (int32_t chunk_z = -1; chunk_z <= 1; ++chunk_z) {
    octaryn_client_chunk_mesh_upload_record old_chunk{};
    old_chunk.chunk_x = -1;
    old_chunk.chunk_y = 0;
    old_chunk.chunk_z = chunk_z;
    old_chunk.opaque_face_count = 1u;
    retained.chunks.push_back(old_chunk);
  }

  world_mesh_upload_frame batch;
  empty_world_stream_mesh_batch_result result;
  build_empty_world_mesh_frame_from_stream_batch(
      stream, block_lookup{}, previous, dirty_columns, retained, 0u, 10u, batch,
      result);

  bool ok = true;
  ok &= expect_equal("retained shift first batch clear columns",
                     result.clear_columns, 3u);
  ok &= expect_equal("retained shift first batch build columns",
                     result.build_columns, 7u);
  ok &= expect_true("retained shift emits clear chunks", !batch.chunks.empty());
  return ok;
}

bool validate_radius32_stream_mesh_batch_is_bounded() {
  constexpr uint32_t kRadius = 32u;
  constexpr size_t kBatchBudget = 12u;
  const server_chunk_stream_file stream = make_radius_stream(kRadius);
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  world_mesh_upload_frame retained;

  world_mesh_upload_frame first_batch;
  empty_world_stream_mesh_batch_result first_result;
  build_empty_world_mesh_frame_from_stream_batch(
      stream, block_lookup{}, previous, dirty_columns, retained, 0u, kBatchBudget,
      first_batch, first_result);

  world_mesh_upload_frame second_batch;
  empty_world_stream_mesh_batch_result second_result;
  build_empty_world_mesh_frame_from_stream_batch(
      stream, block_lookup{}, previous, dirty_columns, retained,
      first_result.next_entry, kBatchBudget, second_batch, second_result);

  bool ok = true;
  ok &= expect_equal("radius32 stream columns", stream.columns.size(), 4225u);
  ok &= expect_equal("radius32 active plan columns",
                     first_result.summary.active_columns, 4225u);
  ok &= expect_equal("radius32 first batch processed",
                     first_result.processed_entries, kBatchBudget);
  ok &= expect_equal("radius32 first batch next entry",
                     first_result.next_entry, kBatchBudget);
  ok &= expect_equal("radius32 first batch remaining",
                     first_result.remaining_entries,
                     stream.columns.size() - kBatchBudget);
  ok &= expect_true("radius32 first batch remains active",
                    !first_result.complete);
  ok &= expect_true("radius32 first batch emits chunks",
                    !first_batch.chunks.empty());
  ok &= expect_equal("radius32 first batch render sections",
                     first_batch.sections.size(), kBatchBudget * 16u);
  ok &= expect_true("radius32 section visibility blocks solid depth",
                    first_batch.sections.front().visibility.face_masks[0] ==
                        0u);
  ok &= expect_equal("radius32 second batch first entry",
                     second_result.first_entry, kBatchBudget);
  ok &= expect_equal("radius32 second batch processed",
                     second_result.processed_entries, kBatchBudget);
  ok &= expect_true("radius32 second batch remains active",
                    !second_result.complete);
  return ok;
}

bool validate_radius32_stream_mesh_batches_complete() {
  constexpr uint32_t kRadius = 32u;
  constexpr size_t kBatchBudget = 12u;
  const server_chunk_stream_file stream = make_radius_stream(kRadius);
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  world_mesh_upload_frame retained;

  size_t cursor = 0u;
  size_t batches = 0u;
  size_t total_processed = 0u;
  size_t total_chunks = 0u;
  size_t max_processed = 0u;
  bool cursor_progressed = true;
  empty_world_stream_mesh_batch_result result{};

  do {
    world_mesh_upload_frame batch;
    const size_t previous_cursor = cursor;
    build_empty_world_mesh_frame_from_stream_batch(
        stream, block_lookup{}, previous, dirty_columns, retained, cursor,
        kBatchBudget, batch, result);

    const bool advanced = result.next_entry > previous_cursor;
    cursor_progressed &= advanced;
    cursor = result.next_entry;
    ++batches;
    total_processed += result.processed_entries;
    total_chunks += batch.chunks.size();
    max_processed = std::max(max_processed, result.processed_entries);
    retained.chunks.insert(retained.chunks.end(), batch.chunks.begin(),
                           batch.chunks.end());
    if (!advanced) {
      break;
    }
  } while (!result.complete && cursor < stream.columns.size());

  bool ok = true;
  ok &= expect_true("radius32 full stream cursor progresses", cursor_progressed);
  ok &= expect_true("radius32 full stream uses many bounded batches",
                    batches > 100u);
  ok &= expect_equal("radius32 full stream processed columns",
                     total_processed, stream.columns.size());
  ok &= expect_equal("radius32 full stream final cursor", cursor,
                     stream.columns.size());
  ok &= expect_true("radius32 full stream final batch completes",
                    result.complete);
  ok &= expect_true("radius32 full stream batch cap respected",
                    max_processed <= kBatchBudget);
  ok &= expect_true("radius32 full stream emits retained-update chunks",
                    total_chunks > stream.columns.size());
  return ok;
}

uint64_t probe_column_key(int32_t chunk_x, int32_t chunk_z) {
  return static_cast<uint32_t>(chunk_x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(chunk_z)) << 32u);
}
world_mesh_upload_frame retained_frame_from_columns(const std::unordered_set<uint64_t> &columns) {
  world_mesh_upload_frame frame;
  for (const uint64_t key : columns) {
    octaryn_client_chunk_mesh_upload_record chunk{};
    chunk.chunk_x = static_cast<int32_t>(static_cast<uint32_t>(key));
    chunk.chunk_y = 0; chunk.chunk_z = static_cast<int32_t>(static_cast<uint32_t>(key >> 32u)); chunk.opaque_face_count = 1u;
    frame.chunks.push_back(chunk);
  }
  return frame;
}
void apply_batch_columns(const world_mesh_upload_frame &batch, std::unordered_set<uint64_t> &columns) {
  for (const octaryn_client_chunk_mesh_upload_record &chunk : batch.chunks) {
    const uint64_t key = probe_column_key(chunk.chunk_x, chunk.chunk_z);
    if (chunk.opaque_face_count == 0u && chunk.transparent_face_count == 0u &&
        chunk.sprite_vertex_count == 0u && chunk.fluid_block_count == 0u) {
      columns.erase(key);
    } else {
      columns.insert(key);
    }
  }
}
bool validate_completed_stream_state_has_no_holes_or_stale_columns() {
  constexpr uint32_t kRadius = 3u;
  constexpr size_t kBudget = 5u;
  std::unordered_set<uint64_t> retained;
  chunk_view previous{0, 0, 0};
  bool ok = true;
  for (uint64_t step = 0; step < 3u; ++step) {
    const auto stream = make_center_stream(static_cast<int32_t>(step), -static_cast<int32_t>(step), kRadius, step + 10u);
    size_t cursor = 0u;
    empty_world_stream_mesh_batch_result result{};
    do {
      world_mesh_upload_frame batch;
      const world_mesh_upload_frame frame = retained_frame_from_columns(retained);
      build_empty_world_mesh_frame_from_stream_batch(
          stream, block_lookup{}, previous, {}, frame, cursor, kBudget, batch, result);
      cursor = result.next_entry;
      apply_batch_columns(batch, retained);
    } while (!result.complete);
    previous = chunk_view_from_server_stream(stream);
    for (const auto &column : stream.columns) {
      ok &= expect_true("completed stream retained active column",
                        retained.contains(probe_column_key(column.chunkX, column.chunkZ)));
    }
    ok &= expect_equal("completed stream retained active count", retained.size(), stream.columns.size());
  }
  return ok;
}
bool validate_full_depth_terrain_mesh() {
  const world_mesh_upload_frame frame = build_frame(block_lookup{});
  bool has_surface_or_above_chunk = false;
  bool has_non_flat_chunk = false;
  for (const octaryn_client_chunk_mesh_upload_record &chunk : frame.chunks) {
    has_surface_or_above_chunk |= chunk.chunk_y >= 0;
    has_non_flat_chunk |= chunk.chunk_y != kEmptyWorldChunkY;
  }

  bool ok = true;
  ok &= expect_true("terrain emits chunks", !frame.chunks.empty());
  ok &= expect_true("terrain is not one-layer flat mesh", has_non_flat_chunk);
  ok &= expect_true("terrain emits surface depth chunks",
                    has_surface_or_above_chunk);
  ok &= expect_true("terrain emits packed faces", !frame.opaque_faces.empty());
  return ok;
}

bool contains_face(const world_mesh_upload_frame &frame, uint64_t face) {
  return std::find(frame.opaque_faces.begin(), frame.opaque_faces.end(), face) !=
         frame.opaque_faces.end();
}

bool validate_air_override_removes_seed_side_face() {
  const world_mesh_upload_frame base = build_frame(block_lookup{});
  for (int32_t z = 1; z < kEmptyWorldChunkSize - 1; ++z) {
    for (int32_t x = 1; x < kEmptyWorldChunkSize - 1; ++x) {
      const empty_world_terrain_column column = empty_world_seed_column(x, z);
      const empty_world_terrain_column neighbor =
          empty_world_seed_column(x + 1, z);
      if (neighbor.height >= column.height) {
        continue;
      }

      constexpr uint32_t direction = 2u;
      const uint64_t stale_face = pack_empty_world_block_face_with_layer(
          static_cast<uint32_t>(x),
          static_cast<uint32_t>(floor_mod_int32(column.height,
                                                kEmptyWorldChunkSize)),
          static_cast<uint32_t>(z), direction, 1u, 1u,
          empty_world_block_atlas_layer(column.surface, direction));
      if (!contains_face(base, stale_face)) {
        continue;
      }

      block_lookup overrides;
      overrides[block_position_key{x, column.height, z}] = 0u;
      const world_mesh_upload_frame edited = build_frame(overrides);
      return expect_true("air override removes seed side face",
                         !contains_face(edited, stale_face));
    }
  }
  return expect_true("terrain side face sample found", false);
}

bool validate_height_edge_top_faces_are_cut() {
  const world_mesh_upload_frame frame = build_frame(block_lookup{});
  size_t checked_edges = 0u;
  for (int32_t z = 1; z < kEmptyWorldChunkSize - 1; ++z) {
    for (int32_t x = 1; x < kEmptyWorldChunkSize - 1; ++x) {
      const empty_world_terrain_column column = empty_world_seed_column(x, z);
      const bool edge =
          empty_world_seed_column(x - 1, z).height != column.height ||
          empty_world_seed_column(x + 1, z).height != column.height ||
          empty_world_seed_column(x, z - 1).height != column.height ||
          empty_world_seed_column(x, z + 1).height != column.height;
      if (!edge) {
        continue;
      }
      const uint64_t top_face = pack_empty_world_block_face_with_layer(
          static_cast<uint32_t>(x),
          static_cast<uint32_t>(floor_mod_int32(column.height,
                                                kEmptyWorldChunkSize)),
          static_cast<uint32_t>(z), 4u, 1u, 1u,
          empty_world_block_atlas_layer(column.surface, 4u));
      if (!contains_face(frame, top_face)) {
        return expect_true("height edge top face is cut", false);
      }
      ++checked_edges;
    }
  }
  return expect_true("height edge top faces checked", checked_edges > 0u);
}

bool validate_placeable_block_atlas_layers() {
  bool ok = true;
  ok &= expect_equal("log side atlas layer",
                     empty_world_block_atlas_layer(6u, 0u), 8u);
  ok &= expect_equal("log top atlas layer",
                     empty_world_block_atlas_layer(6u, 4u), 7u);
  ok &= expect_equal("leaves atlas layer",
                     empty_world_block_atlas_layer(7u, 4u), 10u);
  ok &= expect_equal("yellow torch atlas layer",
                     empty_world_block_atlas_layer(25u, 4u), 20u);
  ok &= expect_equal("planks atlas layer",
                     empty_world_block_atlas_layer(29u, 4u), 24u);
  ok &= expect_equal("glass atlas layer",
                     empty_world_block_atlas_layer(30u, 4u), 25u);
  ok &= expect_equal("lava atlas layer",
                     empty_world_block_atlas_layer(31u, 4u), 27u);
  return ok;
}

bool validate_adjacent_override_face_culling() {
  const world_mesh_upload_frame base = build_frame(block_lookup{});
  block_lookup overrides;
  overrides[block_position_key{4, 200, 4}] = 5u;
  overrides[block_position_key{5, 200, 4}] = 5u;

  const world_mesh_upload_frame edited = build_frame(overrides);
  if (!expect_true("edited frame has extra faces",
                   edited.opaque_faces.size() > base.opaque_faces.size())) {
    return false;
  }

  const size_t added_faces =
      edited.opaque_faces.size() - base.opaque_faces.size();
  return expect_equal("adjacent override exposed faces", added_faces, 10u);
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_full_depth_terrain_mesh();
  ok &= validate_air_override_removes_seed_side_face();
  ok &= validate_height_edge_top_faces_are_cut();
  ok &= validate_placeable_block_atlas_layers();
  ok &= validate_adjacent_override_face_culling();
  ok &= validate_batched_stream_mesh_matches_full_stream();
  ok &= validate_retained_stream_unloads_before_builds();
  ok &= validate_radius32_stream_mesh_batch_is_bounded();
  ok &= validate_radius32_stream_mesh_batches_complete();
  ok &= validate_completed_stream_state_has_no_holes_or_stale_columns();
  if (!ok) {
    return 1;
  }

  std::puts("client empty world mesh probe passed");
  return 0;
}
