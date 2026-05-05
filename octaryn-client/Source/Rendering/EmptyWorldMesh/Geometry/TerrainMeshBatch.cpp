#include "EmptyWorldMesh.h"

#include "Packing.h"
#include "View.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::server_chunk_stream_column_record;
using octaryn_client_app::server_chunk_stream_file;

namespace {

constexpr uint32_t kUploadRecordVersion = 1u;
constexpr uint32_t kUploadRecordSize = 96u;
constexpr uint32_t kClearTransparentFaces = 1u << 1u;
constexpr uint32_t kClearSpriteVertices = 1u << 2u;
constexpr uint32_t kClearFluidBlocks = 1u << 3u;
constexpr int32_t kEmptyWorldMaxChunkY =
    (kEmptyWorldMaxYExclusive - 1) / kEmptyWorldChunkSize;

uint64_t column_key(int32_t chunk_x, int32_t chunk_z) {
  return static_cast<uint32_t>(chunk_x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(chunk_z)) << 32u);
}

void append_clear_chunk(world_mesh_upload_frame &mesh_frame, int32_t chunk_x,
                        int32_t chunk_y, int32_t chunk_z) {
  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.version = kUploadRecordVersion;
  chunk.size = kUploadRecordSize;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.chunk_z = chunk_z;
  chunk.flags =
      kClearTransparentFaces | kClearSpriteVertices | kClearFluidBlocks;
  mesh_frame.chunks.push_back(chunk);
}

void append_clear_column(world_mesh_upload_frame &mesh_frame, int32_t chunk_x,
                         int32_t chunk_z) {
  for (int32_t chunk_y = kEmptyWorldMinChunkY; chunk_y <= kEmptyWorldMaxChunkY;
       ++chunk_y) {
    append_clear_chunk(mesh_frame, chunk_x, chunk_y, chunk_z);
  }
}

void append_upload_frame(world_mesh_upload_frame &destination,
                         const world_mesh_upload_frame &source) {
  const size_t opaque_offset = destination.opaque_faces.size();
  const size_t transparent_offset = destination.transparent_faces.size();
  const size_t sprite_offset = destination.sprite_vertices.size();

  destination.opaque_faces.insert(destination.opaque_faces.end(),
                                  source.opaque_faces.begin(),
                                  source.opaque_faces.end());
  destination.transparent_faces.insert(destination.transparent_faces.end(),
                                       source.transparent_faces.begin(),
                                       source.transparent_faces.end());
  destination.sprite_vertices.insert(destination.sprite_vertices.end(),
                                     source.sprite_vertices.begin(),
                                     source.sprite_vertices.end());
  destination.opaque_bytes += source.opaque_bytes;
  destination.transparent_bytes += source.transparent_bytes;
  destination.sprite_bytes += source.sprite_bytes;
  destination.fluid_blocks += source.fluid_blocks;

  for (octaryn_client_chunk_mesh_upload_record chunk : source.chunks) {
    chunk.opaque_face_offset += opaque_offset;
    chunk.transparent_face_offset += transparent_offset;
    chunk.sprite_vertex_offset += sprite_offset;
    destination.chunks.push_back(chunk);
  }
}

bool dirty_column_contains(
    const std::vector<empty_world_dirty_column> &dirty_columns, int32_t chunk_x,
    int32_t chunk_z) {
  for (const empty_world_dirty_column &column : dirty_columns) {
    if (column.chunk_x == chunk_x && column.chunk_z == chunk_z) {
      return true;
    }
  }
  return false;
}

bool override_column_contains(const block_lookup &overrides, int32_t chunk_x,
                              int32_t chunk_z) {
  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (floor_div_int32(key.x, kEmptyWorldChunkSize) == chunk_x &&
        floor_div_int32(key.z, kEmptyWorldChunkSize) == chunk_z) {
      return true;
    }
  }
  return false;
}

bool entry_needs_batch(const chunk_mesh_plan_entry &entry,
                       const block_lookup &overrides,
                       const std::vector<empty_world_dirty_column> &dirty) {
  return entry.action != chunk_mesh_plan_action::preserve ||
         dirty_column_contains(dirty, entry.chunk_x, entry.chunk_z) ||
         override_column_contains(overrides, entry.chunk_x, entry.chunk_z);
}

void select_entries(const chunk_mesh_plan &plan, const block_lookup &overrides,
                    const std::vector<empty_world_dirty_column> &dirty_columns,
                    size_t first_entry, size_t max_entries,
                    std::vector<chunk_mesh_plan_entry> &selected,
                    empty_world_stream_mesh_batch_result &result) {
  result.first_entry = std::min(first_entry, plan.entries.size());
  result.next_entry = result.first_entry;
  while (result.next_entry < plan.entries.size() &&
         result.processed_entries < max_entries) {
    const chunk_mesh_plan_entry &entry = plan.entries[result.next_entry];
    ++result.next_entry;
    if (!entry_needs_batch(entry, overrides, dirty_columns)) {
      continue;
    }

    selected.push_back(entry);
    ++result.processed_entries;
    if (entry.action == chunk_mesh_plan_action::clear) {
      ++result.clear_columns;
    } else {
      ++result.build_columns;
    }
  }

  result.remaining_entries = plan.entries.size() - result.next_entry;
  result.complete = result.next_entry >= plan.entries.size();
}

server_chunk_stream_file
make_selected_stream(const server_chunk_stream_file &stream,
                     const std::vector<chunk_mesh_plan_entry> &selected) {
  std::unordered_set<uint64_t> selected_columns;
  for (const chunk_mesh_plan_entry &entry : selected) {
    if (entry.action != chunk_mesh_plan_action::clear) {
      selected_columns.insert(column_key(entry.chunk_x, entry.chunk_z));
    }
  }

  server_chunk_stream_file selected_stream = stream;
  selected_stream.columns.clear();
  selected_stream.columns.reserve(selected_columns.size());
  for (const server_chunk_stream_column_record &column : stream.columns) {
    if (selected_columns.contains(column_key(column.chunkX, column.chunkZ))) {
      selected_stream.columns.push_back(column);
    }
  }
  return selected_stream;
}

block_lookup filter_overrides_for_selected_columns(
    const block_lookup &overrides,
    const std::vector<chunk_mesh_plan_entry> &selected) {
  std::unordered_set<uint64_t> selected_columns;
  for (const chunk_mesh_plan_entry &entry : selected) {
    if (entry.action != chunk_mesh_plan_action::clear) {
      selected_columns.insert(column_key(entry.chunk_x, entry.chunk_z));
    }
  }

  block_lookup filtered;
  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    const uint64_t key_column =
        column_key(floor_div_int32(key.x, kEmptyWorldChunkSize),
                   floor_div_int32(key.z, kEmptyWorldChunkSize));
    if (selected_columns.contains(key_column)) {
      filtered.insert(entry);
    }
  }
  return filtered;
}

std::vector<empty_world_dirty_column>
filter_dirty_columns(const std::vector<empty_world_dirty_column> &dirty_columns,
                     const std::vector<chunk_mesh_plan_entry> &selected) {
  std::unordered_set<uint64_t> selected_columns;
  for (const chunk_mesh_plan_entry &entry : selected) {
    if (entry.action != chunk_mesh_plan_action::clear) {
      selected_columns.insert(column_key(entry.chunk_x, entry.chunk_z));
    }
  }

  std::vector<empty_world_dirty_column> filtered;
  for (const empty_world_dirty_column &column : dirty_columns) {
    if (selected_columns.contains(column_key(column.chunk_x, column.chunk_z))) {
      filtered.push_back(column);
    }
  }
  return filtered;
}

} // namespace

void build_empty_world_mesh_frame_from_stream_batch(
    const server_chunk_stream_file &stream, const block_lookup &overrides,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    size_t first_plan_entry, size_t max_plan_entries,
    world_mesh_upload_frame &mesh_frame,
    empty_world_stream_mesh_batch_result &batch_result) {
  mesh_frame = {};
  batch_result = {};
  const chunk_view stream_view = chunk_view_from_server_stream(stream);
  const chunk_mesh_plan plan =
      build_chunk_mesh_plan(previous_chunk_view, stream_view,
                            chunk_mesh_plan_default_options(stream_view));
  batch_result.summary = plan.summary;

  std::vector<chunk_mesh_plan_entry> selected;
  select_entries(plan, overrides, dirty_columns, first_plan_entry,
                 std::max<size_t>(max_plan_entries, 1u), selected,
                 batch_result);
  if (selected.empty()) {
    return;
  }

  for (const chunk_mesh_plan_entry &entry : selected) {
    const bool dirty =
        dirty_column_contains(dirty_columns, entry.chunk_x, entry.chunk_z) ||
        override_column_contains(overrides, entry.chunk_x, entry.chunk_z);
    if (entry.action == chunk_mesh_plan_action::clear || dirty) {
      append_clear_column(mesh_frame, entry.chunk_x, entry.chunk_z);
    }
  }

  const server_chunk_stream_file selected_stream =
      make_selected_stream(stream, selected);
  if (!selected_stream.columns.empty()) {
    const block_lookup selected_overrides =
        filter_overrides_for_selected_columns(overrides, selected);
    const std::vector<empty_world_dirty_column> selected_dirty_columns =
        filter_dirty_columns(dirty_columns, selected);
    world_mesh_upload_frame geometry_frame{};
    const chunk_view empty_previous_view{};
    build_empty_world_mesh_frame_from_stream(
        selected_stream, selected_overrides, empty_previous_view,
        selected_dirty_columns, geometry_frame);
    append_upload_frame(mesh_frame, geometry_frame);
  }
}
