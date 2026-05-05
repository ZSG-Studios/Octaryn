#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API __attribute__((visibility("default")))
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

struct octaryn_server_persistence_plan_counts {
  uint32_t column_count;
  uint32_t block_count;
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

}
