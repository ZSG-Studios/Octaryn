#include "EmptyWorldMesh.h"
#include "Log.h"
#include "Packing.h"

#include <cstdio>
#include <string_view>

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

world_mesh_upload_frame build_frame(const block_lookup &overrides) {
  world_mesh_upload_frame frame;
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  build_empty_world_mesh_frame_from_stream(
      make_single_column_stream(), overrides, previous, dirty_columns, frame);
  return frame;
}

bool validate_batched_stream_mesh_matches_full_stream() {
  const server_chunk_stream_file stream = make_radius_stream(1u);
  const chunk_view previous{0, 0, 0};
  const std::vector<empty_world_dirty_column> dirty_columns;
  world_mesh_upload_frame full;
  build_empty_world_mesh_frame_from_stream(stream, block_lookup{}, previous,
                                           dirty_columns, full);

  world_mesh_upload_frame batched;
  empty_world_stream_mesh_batch_result result;
  size_t cursor = 0u;
  size_t batches = 0u;
  do {
    world_mesh_upload_frame batch;
    build_empty_world_mesh_frame_from_stream_batch(stream, block_lookup{},
                                                   previous, dirty_columns,
                                                   cursor, 2u, batch, result);
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
  ok &= validate_adjacent_override_face_culling();
  ok &= validate_batched_stream_mesh_matches_full_stream();
  if (!ok) {
    return 1;
  }

  std::puts("client empty world mesh probe passed");
  return 0;
}
