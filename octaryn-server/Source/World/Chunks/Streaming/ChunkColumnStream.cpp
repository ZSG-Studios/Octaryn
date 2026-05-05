#include "ChunkColumnStream.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace {

using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockStore;
using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;

enum window_event_kind : uint32_t {
  window_event_load = 0u,
  window_event_preserve = 1u,
  window_event_unload = 2u,
};

using chunk_key = std::pair<int32_t, int32_t>;

BlockStore *as_store(void *store) { return static_cast<BlockStore *>(store); }

std::set<chunk_key> window_set(int32_t center_chunk_x, int32_t center_chunk_z,
                               uint32_t radius) {
  std::set<chunk_key> chunks;
  const auto radius_int = static_cast<int32_t>(radius);
  for (int32_t chunk_x = center_chunk_x - radius_int;
       chunk_x <= center_chunk_x + radius_int; ++chunk_x) {
    for (int32_t chunk_z = center_chunk_z - radius_int;
         chunk_z <= center_chunk_z + radius_int; ++chunk_z) {
      chunks.emplace(chunk_x, chunk_z);
    }
  }
  return chunks;
}

std::vector<octaryn_server_chunk_window_event>
build_window_events(const std::set<chunk_key> &current,
                    const std::set<chunk_key> &previous) {
  std::vector<octaryn_server_chunk_window_event> events;
  events.reserve(current.size() + previous.size());
  for (const auto &[chunk_x, chunk_z] : current) {
    events.push_back(octaryn_server_chunk_window_event{
        .kind = previous.contains({chunk_x, chunk_z}) ? window_event_preserve
                                                      : window_event_load,
        .chunk_x = chunk_x,
        .chunk_z = chunk_z,
    });
  }
  for (const auto &[chunk_x, chunk_z] : previous) {
    if (!current.contains({chunk_x, chunk_z})) {
      events.push_back(octaryn_server_chunk_window_event{
          .kind = window_event_unload,
          .chunk_x = chunk_x,
          .chunk_z = chunk_z,
      });
    }
  }
  return events;
}

bool is_loaded_column(const std::vector<octaryn_server_chunk_window_event> &events,
                      int32_t chunk_x, int32_t chunk_z) {
  return std::any_of(events.begin(), events.end(), [chunk_x, chunk_z](
                                                   const auto &event) {
    return event.kind == window_event_load && event.chunk_x == chunk_x &&
           event.chunk_z == chunk_z;
  });
}

std::vector<BlockEdit> column_edits(BlockStore *store, int32_t chunk_x,
                                    int32_t chunk_z, bool metadata_only,
                                    const std::vector<
                                        octaryn_server_chunk_window_event>
                                        &events) {
  if (store == nullptr ||
      (metadata_only && !is_loaded_column(events, chunk_x, chunk_z))) {
    return {};
  }

  return store->snapshot_chunk_column(chunk_x * ChunkWidth, chunk_z * ChunkDepth);
}

} // namespace

extern "C" {

int32_t octaryn_server_chunk_stream_count(
    void *store, int32_t center_chunk_x, int32_t center_chunk_z,
    uint32_t radius, uint32_t has_previous_window,
    int32_t previous_center_chunk_x, int32_t previous_center_chunk_z,
    uint32_t previous_radius, uint32_t metadata_only,
    octaryn_server_chunk_stream_counts *counts) {
  if (counts == nullptr) {
    return -1;
  }

  auto *block_store = as_store(store);
  if (block_store == nullptr) {
    *counts = {};
    return -1;
  }

  const auto current = window_set(center_chunk_x, center_chunk_z, radius);
  const auto previous = has_previous_window != 0u
                            ? window_set(previous_center_chunk_x,
                                         previous_center_chunk_z,
                                         previous_radius)
                            : std::set<chunk_key>{};
  const auto events = build_window_events(current, previous);
  uint32_t block_count = 0u;
  for (const auto &[chunk_x, chunk_z] : current) {
    block_count += static_cast<uint32_t>(
        column_edits(block_store, chunk_x, chunk_z, metadata_only != 0u, events)
            .size());
  }

  *counts = octaryn_server_chunk_stream_counts{
      .event_count = static_cast<uint32_t>(events.size()),
      .column_count = static_cast<uint32_t>(current.size()),
      .block_count = block_count,
  };
  return 0;
}

int32_t octaryn_server_chunk_stream_fill(
    void *store, int32_t center_chunk_x, int32_t center_chunk_z,
    uint32_t radius, uint32_t has_previous_window,
    int32_t previous_center_chunk_x, int32_t previous_center_chunk_z,
    uint32_t previous_radius, uint32_t metadata_only,
    octaryn_server_chunk_window_event *events, uint32_t event_capacity,
    octaryn_server_chunk_stream_column *columns, uint32_t column_capacity,
    octaryn_server_chunk_stream_block *blocks, uint32_t block_capacity,
    octaryn_server_chunk_stream_counts *written) {
  auto *block_store = as_store(store);
  if (block_store == nullptr || written == nullptr) {
    return -1;
  }

  const auto current = window_set(center_chunk_x, center_chunk_z, radius);
  const auto previous = has_previous_window != 0u
                            ? window_set(previous_center_chunk_x,
                                         previous_center_chunk_z,
                                         previous_radius)
                            : std::set<chunk_key>{};
  const auto native_events = build_window_events(current, previous);
  octaryn_server_chunk_stream_counts counts = {};
  if (octaryn_server_chunk_stream_count(
          store, center_chunk_x, center_chunk_z, radius, has_previous_window,
          previous_center_chunk_x, previous_center_chunk_z, previous_radius,
          metadata_only, &counts) != 0) {
    return -1;
  }

  if (event_capacity < counts.event_count ||
      column_capacity < counts.column_count ||
      block_capacity < counts.block_count) {
    *written = counts;
    return -2;
  }

  if ((counts.event_count != 0u && events == nullptr) ||
      (counts.column_count != 0u && columns == nullptr) ||
      (counts.block_count != 0u && blocks == nullptr)) {
    *written = counts;
    return -1;
  }

  for (uint32_t index = 0; index < counts.event_count; ++index) {
    events[index] = native_events[index];
  }

  uint32_t column_index = 0u;
  uint32_t block_index = 0u;
  const auto radius_int = static_cast<int32_t>(radius);
  for (int32_t chunk_z = center_chunk_z - radius_int;
       chunk_z <= center_chunk_z + radius_int; ++chunk_z) {
    for (int32_t chunk_x = center_chunk_x - radius_int;
         chunk_x <= center_chunk_x + radius_int; ++chunk_x) {
      const uint32_t offset = block_index;
      const auto edits = column_edits(block_store, chunk_x, chunk_z,
                                      metadata_only != 0u, native_events);
      for (const BlockEdit &edit : edits) {
        blocks[block_index++] = octaryn_server_chunk_stream_block{
            .x = edit.position.x,
            .y = edit.position.y,
            .z = edit.position.z,
            .block = edit.block,
        };
      }

      columns[column_index++] = octaryn_server_chunk_stream_column{
          .chunk_x = chunk_x,
          .chunk_z = chunk_z,
          .origin_x = chunk_x * ChunkWidth,
          .origin_z = chunk_z * ChunkDepth,
          .block_offset = offset,
          .block_count = static_cast<uint32_t>(edits.size()),
      };
    }
  }

  *written = counts;
  return 0;
}

}
