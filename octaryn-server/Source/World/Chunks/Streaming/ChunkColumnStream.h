#pragma once

#include "BlockStore.h"

#include <cstdint>

extern "C" {

struct octaryn_server_chunk_window_event {
  uint32_t kind;
  int32_t chunk_x;
  int32_t chunk_z;
};

struct octaryn_server_chunk_stream_column {
  int32_t chunk_x;
  int32_t chunk_z;
  int32_t origin_x;
  int32_t origin_z;
  uint32_t block_offset;
  uint32_t block_count;
};

struct octaryn_server_chunk_stream_block {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct octaryn_server_chunk_stream_counts {
  uint32_t event_count;
  uint32_t column_count;
  uint32_t block_count;
};

struct octaryn_server_chunk_stream_snapshot_request {
  const char *stream_path;
  uint64_t epoch;
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  uint32_t radius;
  uint32_t has_previous_window;
  int32_t previous_center_chunk_x;
  int32_t previous_center_chunk_z;
  uint32_t previous_radius;
  uint32_t metadata_only;
  uint64_t world_seed;
  uint64_t world_time_day_index;
  uint32_t world_time_second_of_day;
  double world_time_total_seconds;
  float world_time_day_fraction;
  float player_x;
  float player_y;
  float player_z;
  float player_pitch;
  float player_yaw;
  float player_velocity_x;
  float player_velocity_y;
  float player_velocity_z;
  uint32_t player_control_mode;
  uint32_t player_on_ground;
};

struct octaryn_server_chunk_stream_snapshot_result {
  octaryn_server_chunk_stream_counts counts;
  uint32_t load_count;
  uint32_t preserve_count;
  uint32_t unload_count;
};

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_chunk_stream_count(void *store, int32_t center_chunk_x,
                                  int32_t center_chunk_z, uint32_t radius,
                                  uint32_t has_previous_window,
                                  int32_t previous_center_chunk_x,
                                  int32_t previous_center_chunk_z,
                                  uint32_t previous_radius,
                                  uint32_t metadata_only,
                                  octaryn_server_chunk_stream_counts *counts);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_chunk_stream_fill(
    void *store, int32_t center_chunk_x, int32_t center_chunk_z,
    uint32_t radius, uint32_t has_previous_window,
    int32_t previous_center_chunk_x, int32_t previous_center_chunk_z,
    uint32_t previous_radius, uint32_t metadata_only,
    octaryn_server_chunk_window_event *events, uint32_t event_capacity,
    octaryn_server_chunk_stream_column *columns, uint32_t column_capacity,
    octaryn_server_chunk_stream_block *blocks, uint32_t block_capacity,
    octaryn_server_chunk_stream_counts *written);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_chunk_stream_write_snapshot_file(
    void *store, const octaryn_server_chunk_stream_snapshot_request *request,
    octaryn_server_chunk_stream_snapshot_result *result);

}
