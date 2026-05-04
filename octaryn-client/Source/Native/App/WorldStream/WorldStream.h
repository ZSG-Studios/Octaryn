#pragma once

#include "JsonFiles.h"
#include "PresentationState.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace octaryn_client_app {

struct server_world_time_state {
  bool active = false;
  uint64_t day_index = 0u;
  uint32_t second_of_day = 43200u;
  double total_seconds = 43200.0;
  float day_fraction = 0.5f;
};

bool apply_blocks_from_records(const std::vector<world_block_record> &records,
                               bool spawn_only,
                               std::vector<presentation_block> &blocks);
bool apply_top_blocks_from_records(
    const std::vector<world_block_record> &records, bool spawn_only,
    std::vector<presentation_block> &blocks);
bool load_world_snapshot_blocks(
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks,
    server_world_time_state &world_time);
bool load_world_blocks_from_path(
    const std::filesystem::path &path,
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks);
bool load_server_chunk_stream_file(server_chunk_stream_file &stream,
                                   server_world_time_state &world_time,
                                   bool missing_is_waiting);

} // namespace octaryn_client_app
