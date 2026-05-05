#include "ChunkColumnStream.h"

namespace {

using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockStore;
using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;

constexpr uint32_t max_request_radius = 32u;

int32_t write_request_result(octaryn_chunk_column_request_frame *request,
                             uint32_t column_count, uint32_t block_count,
                             uint32_t status) {
  *request = octaryn_chunk_column_request_frame{
      .version = 1u,
      .size = OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE,
      .center_chunk_x = request->center_chunk_x,
      .center_chunk_z = request->center_chunk_z,
      .radius = request->radius,
      .column_capacity = request->column_capacity,
      .block_capacity = request->block_capacity,
      .column_count = column_count,
      .block_count = block_count,
      .status = status,
      .columns_address = request->columns_address,
      .blocks_address = request->blocks_address,
  };
  return status == 0u ? 0 : -static_cast<int32_t>(status);
}

} // namespace

extern "C" {

int32_t octaryn_server_chunk_stream_request_columns(
    void *store, octaryn_chunk_column_request_frame *request_frame) {
  if (request_frame == nullptr || request_frame->version != 1u ||
      request_frame->size != OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE) {
    return -1;
  }
  if (request_frame->radius > max_request_radius) {
    return write_request_result(request_frame, 0u, 0u, 2u);
  }

  octaryn_server_chunk_stream_counts counts{};
  if (octaryn_server_chunk_stream_count(
          store, request_frame->center_chunk_x, request_frame->center_chunk_z,
          request_frame->radius, 0u, 0, 0, 0u, 0u, &counts) != 0) {
    return -1;
  }
  if (request_frame->column_capacity < counts.column_count) {
    return write_request_result(request_frame, counts.column_count, 0u, 3u);
  }
  if (request_frame->block_capacity < counts.block_count) {
    return write_request_result(request_frame, counts.column_count,
                                counts.block_count, 4u);
  }
  if (request_frame->columns_address == 0u ||
      (counts.block_count != 0u && request_frame->blocks_address == 0u)) {
    return -1;
  }

  auto *columns = reinterpret_cast<octaryn_chunk_column_snapshot_column *>(
      static_cast<uintptr_t>(request_frame->columns_address));
  auto *blocks = reinterpret_cast<octaryn_chunk_column_snapshot_block *>(
      static_cast<uintptr_t>(request_frame->blocks_address));
  auto *block_store = static_cast<BlockStore *>(store);
  uint32_t column_index = 0u;
  uint32_t block_index = 0u;
  const auto radius_int = static_cast<int32_t>(request_frame->radius);
  for (int32_t chunk_z = request_frame->center_chunk_z - radius_int;
       chunk_z <= request_frame->center_chunk_z + radius_int; ++chunk_z) {
    for (int32_t chunk_x = request_frame->center_chunk_x - radius_int;
         chunk_x <= request_frame->center_chunk_x + radius_int; ++chunk_x) {
      const uint32_t offset = block_index;
      const auto edits = block_store->snapshot_chunk_column(
          chunk_x * ChunkWidth, chunk_z * ChunkDepth);
      for (const BlockEdit &edit : edits) {
        blocks[block_index++] = octaryn_chunk_column_snapshot_block{
            .version = 1u,
            .size = OCTARYN_CHUNK_COLUMN_SNAPSHOT_BLOCK_SIZE,
            .x = edit.position.x,
            .y = edit.position.y,
            .z = edit.position.z,
            .block = edit.block,
            .reserved = 0u,
        };
      }

      columns[column_index++] = octaryn_chunk_column_snapshot_column{
          .version = 1u,
          .size = OCTARYN_CHUNK_COLUMN_SNAPSHOT_COLUMN_SIZE,
          .chunk_x = chunk_x,
          .chunk_z = chunk_z,
          .origin_x = chunk_x * ChunkWidth,
          .origin_z = chunk_z * ChunkDepth,
          .block_offset = offset,
          .block_count = static_cast<uint32_t>(edits.size()),
      };
    }
  }

  return write_request_result(request_frame, column_index, block_index, 0u);
}
}
