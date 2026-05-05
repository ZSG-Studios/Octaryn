#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API                                   \
  __attribute__((visibility("default")))
#endif

extern "C" {

struct octaryn_server_persistence_block_position {
  int32_t x;
  int32_t y;
  int32_t z;
};

struct octaryn_server_persistence_block_edit {
  octaryn_server_persistence_block_position position;
  uint16_t block;
};

struct octaryn_server_persistence_chunk_column {
  int32_t origin_x;
  int32_t origin_z;
  uint32_t block_offset;
  uint32_t block_count;
};

struct octaryn_server_persistence_chunk_override_block {
  int32_t bx;
  int32_t by;
  int32_t bz;
  uint16_t block;
};

struct octaryn_server_persistence_chunk_override_file {
  uint32_t version;
  int32_t cx;
  int32_t cz;
  uint32_t block_count;
};

struct octaryn_server_persistence_world_block_override_file {
  uint32_t version;
  uint32_t block_count;
};

struct octaryn_server_persistence_plan_counts {
  uint32_t column_count;
  uint32_t block_count;
};

struct octaryn_server_persistence_player_state {
  float x;
  float y;
  float z;
  float pitch;
  float yaw;
  uint16_t block;
};

struct octaryn_server_persistence_world_time_state {
  uint32_t version;
  uint64_t day_index;
  double seconds_of_day;
};

struct octaryn_server_persistence_world_metadata {
  uint32_t save_exists;
  uint32_t has_world_time;
  uint32_t has_player_data;
  uint32_t has_world_data;
  int32_t player_count;
  int32_t chunk_override_count;
};

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_plan_chunk_columns_count(
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count,
    octaryn_server_persistence_plan_counts *counts);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_plan_chunk_columns_fill(
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count,
    octaryn_server_persistence_chunk_column *columns, uint32_t column_capacity,
    octaryn_server_persistence_block_edit *ordered_edits,
    uint32_t edit_capacity, octaryn_server_persistence_plan_counts *written);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_chunk_override_file_count(
    const char *path, octaryn_server_persistence_chunk_override_file *file);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_chunk_override_file_fill(
    const char *path, octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_capacity,
    octaryn_server_persistence_chunk_override_file *file);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_chunk_override_file(
    const char *path,
    const octaryn_server_persistence_chunk_override_file *file,
    const octaryn_server_persistence_chunk_override_block *blocks);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_world_block_override_file_count(
    const char *path,
    octaryn_server_persistence_world_block_override_file *file);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_world_block_override_file_fill(
    const char *path, octaryn_server_persistence_block_edit *blocks,
    uint32_t block_capacity,
    octaryn_server_persistence_world_block_override_file *file);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_world_block_override_file(
    const char *path,
    const octaryn_server_persistence_world_block_override_file *file,
    const octaryn_server_persistence_block_edit *blocks);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_gzip_file(const char *path,
                                           const uint8_t *payload,
                                           uint64_t payload_size);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_gzip_file_count(const char *path,
                                                uint64_t *payload_size);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_gzip_file_fill(const char *path,
                                               uint8_t *payload,
                                               uint64_t payload_capacity,
                                               uint64_t *payload_size);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_player_file(
    const char *path, octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_player_file(
    const char *path, const octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_world_time_file(
    const char *path, octaryn_server_persistence_world_time_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_world_time_file(
    const char *path, const octaryn_server_persistence_world_time_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_world_metadata_file(
    const char *path, octaryn_server_persistence_world_metadata *metadata);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_world_metadata_file(
    const char *path,
    const octaryn_server_persistence_world_metadata *metadata);
}
