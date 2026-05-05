#include "ChunkColumnStream.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <string>
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

const char *event_kind_name(uint32_t kind) {
  switch (kind) {
  case window_event_preserve:
    return "preserve";
  case window_event_unload:
    return "unload";
  case window_event_load:
  default:
    return "load";
  }
}

const char *control_mode_name(uint32_t mode) {
  return mode == 1u ? "fly" : "walk";
}

bool write_snapshot_file(
    const char *stream_path,
    const octaryn_server_chunk_stream_snapshot_request &request,
    const std::vector<octaryn_server_chunk_window_event> &events,
    const std::vector<octaryn_server_chunk_stream_column> &columns,
    const std::vector<octaryn_server_chunk_stream_block> &blocks,
    const octaryn_server_chunk_stream_snapshot_result &result) {
  if (stream_path == nullptr || stream_path[0] == '\0') {
    return false;
  }

  const std::filesystem::path output_path{stream_path};
  if (output_path.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(output_path.parent_path(), error);
    if (error) {
      return false;
    }
  }

  const std::filesystem::path temporary_path{output_path.string() + ".tmp"};
  std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
  if (!output) {
    return false;
  }

  output << std::boolalpha << std::setprecision(9);
  output << "{\n"
         << "  \"version\": 1,\n"
         << "  \"epoch\": " << request.epoch << ",\n"
         << "  \"source\": \"server_process_chunk_stream\",\n"
         << "  \"centerChunkX\": " << request.center_chunk_x << ",\n"
         << "  \"centerChunkZ\": " << request.center_chunk_z << ",\n"
         << "  \"radius\": " << request.radius << ",\n"
         << "  \"worldSeed\": " << request.world_seed << ",\n"
         << "  \"worldTimeDayIndex\": " << request.world_time_day_index
         << ",\n"
         << "  \"worldTimeSecondOfDay\": "
         << request.world_time_second_of_day << ",\n"
         << "  \"worldTimeTotalSeconds\": "
         << request.world_time_total_seconds << ",\n"
         << "  \"worldTimeDayFraction\": "
         << request.world_time_day_fraction << ",\n"
         << "  \"playerStateSource\": \"server_authority\",\n"
         << "  \"playerX\": " << request.player_x << ",\n"
         << "  \"playerY\": " << request.player_y << ",\n"
         << "  \"playerZ\": " << request.player_z << ",\n"
         << "  \"playerPitch\": " << request.player_pitch << ",\n"
         << "  \"playerYaw\": " << request.player_yaw << ",\n"
         << "  \"playerVelocityX\": " << request.player_velocity_x << ",\n"
         << "  \"playerVelocityY\": " << request.player_velocity_y << ",\n"
         << "  \"playerVelocityZ\": " << request.player_velocity_z << ",\n"
         << "  \"playerControlMode\": \""
         << control_mode_name(request.player_control_mode) << "\",\n"
         << "  \"playerOnGround\": "
         << (request.player_on_ground != 0u) << ",\n"
         << "  \"windowEpoch\": " << request.epoch << ",\n"
         << "  \"windowLoadCount\": " << result.load_count << ",\n"
         << "  \"windowPreserveCount\": " << result.preserve_count << ",\n"
         << "  \"windowUnloadCount\": " << result.unload_count << ",\n"
         << "  \"windowEvents\": [\n";

  for (size_t index = 0; index < events.size(); ++index) {
    const auto &event = events[index];
    output << "    {\"kind\": \"" << event_kind_name(event.kind)
           << "\", \"chunkX\": " << event.chunk_x
           << ", \"chunkZ\": " << event.chunk_z << "}";
    output << (index + 1u == events.size() ? "\n" : ",\n");
  }

  output << "  ],\n  \"columns\": [\n";
  for (size_t index = 0; index < columns.size(); ++index) {
    const auto &column = columns[index];
    output << "    {\"chunkX\": " << column.chunk_x
           << ", \"chunkZ\": " << column.chunk_z
           << ", \"originX\": " << column.origin_x
           << ", \"originZ\": " << column.origin_z
           << ", \"blockOffset\": " << column.block_offset
           << ", \"blockCount\": " << column.block_count << "}";
    output << (index + 1u == columns.size() ? "\n" : ",\n");
  }

  output << "  ],\n  \"blocks\": [\n";
  for (size_t index = 0; index < blocks.size(); ++index) {
    const auto &block = blocks[index];
    output << "    {\"x\": " << block.x << ", \"y\": " << block.y
           << ", \"z\": " << block.z << ", \"block\": " << block.block
           << "}";
    output << (index + 1u == blocks.size() ? "\n" : ",\n");
  }

  output << "  ]\n}\n";
  output.close();
  if (!output) {
    return false;
  }

  std::error_code error;
  std::filesystem::rename(temporary_path, output_path, error);
  if (!error) {
    return true;
  }

  std::filesystem::remove(output_path, error);
  error.clear();
  std::filesystem::rename(temporary_path, output_path, error);
  return !error;
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

  const bool write_events = events != nullptr;
  if ((write_events && event_capacity < counts.event_count) ||
      column_capacity < counts.column_count ||
      block_capacity < counts.block_count) {
    *written = counts;
    return -2;
  }

  if ((counts.column_count != 0u && columns == nullptr) ||
      (counts.block_count != 0u && blocks == nullptr)) {
    *written = counts;
    return -1;
  }

  if (write_events) {
    for (uint32_t index = 0; index < counts.event_count; ++index) {
      events[index] = native_events[index];
    }
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

int32_t octaryn_server_chunk_stream_write_snapshot_file(
    void *store, const octaryn_server_chunk_stream_snapshot_request *request,
    octaryn_server_chunk_stream_snapshot_result *result) {
  if (request == nullptr || result == nullptr) {
    return -1;
  }

  octaryn_server_chunk_stream_counts counts{};
  if (octaryn_server_chunk_stream_count(
          store, request->center_chunk_x, request->center_chunk_z,
          request->radius, request->has_previous_window,
          request->previous_center_chunk_x, request->previous_center_chunk_z,
          request->previous_radius, request->metadata_only, &counts) != 0) {
    return -1;
  }

  std::vector<octaryn_server_chunk_window_event> events(counts.event_count);
  std::vector<octaryn_server_chunk_stream_column> columns(counts.column_count);
  std::vector<octaryn_server_chunk_stream_block> blocks(counts.block_count);
  octaryn_server_chunk_stream_counts written{};
  if (octaryn_server_chunk_stream_fill(
          store, request->center_chunk_x, request->center_chunk_z,
          request->radius, request->has_previous_window,
          request->previous_center_chunk_x, request->previous_center_chunk_z,
          request->previous_radius, request->metadata_only, events.data(),
          static_cast<uint32_t>(events.size()), columns.data(),
          static_cast<uint32_t>(columns.size()), blocks.data(),
          static_cast<uint32_t>(blocks.size()), &written) != 0) {
    return -1;
  }

  octaryn_server_chunk_stream_snapshot_result snapshot_result{
      .counts = written,
      .load_count = 0u,
      .preserve_count = 0u,
      .unload_count = 0u,
  };
  for (const auto &event : events) {
    switch (event.kind) {
    case window_event_preserve:
      ++snapshot_result.preserve_count;
      break;
    case window_event_unload:
      ++snapshot_result.unload_count;
      break;
    case window_event_load:
    default:
      ++snapshot_result.load_count;
      break;
    }
  }

  if (!write_snapshot_file(request->stream_path, *request, events, columns,
                           blocks, snapshot_result)) {
    return -1;
  }

  *result = snapshot_result;
  return 0;
}

}
