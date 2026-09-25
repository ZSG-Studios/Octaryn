#include "BlockStore.h"
#include "ChunkColumnStream.h"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using namespace octaryn::server::world::blocks;

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

BlockEdit edit(int32_t x, int32_t y, int32_t z, uint16_t block) {
  return BlockEdit{.position = BlockPosition{.x = x, .y = y, .z = z},
                   .block = block};
}

void reset_request(octaryn_chunk_column_request_frame &request,
                   std::vector<octaryn_chunk_column_snapshot_column> &columns,
                   std::vector<octaryn_chunk_column_snapshot_block> &blocks) {
  request = octaryn_chunk_column_request_frame{
      .version = 1u,
      .size = OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE,
      .center_chunk_x = 0,
      .center_chunk_z = 0,
      .radius = 1u,
      .column_capacity = static_cast<uint32_t>(columns.size()),
      .block_capacity = static_cast<uint32_t>(blocks.size()),
      .column_count = 0u,
      .block_count = 0u,
      .status = 0u,
      .columns_address = reinterpret_cast<uint64_t>(columns.data()),
      .blocks_address = reinterpret_cast<uint64_t>(blocks.data()),
  };
}

bool validate_successful_request(
    BlockStore &store, octaryn_chunk_column_request_frame &request,
    const std::vector<octaryn_chunk_column_snapshot_column> &columns,
    const std::vector<octaryn_chunk_column_snapshot_block> &blocks) {
  bool ok = true;
  ok &= expect_equal(
      "chunk request result",
      octaryn_server_chunk_stream_request_columns(&store, &request), 0);
  ok &= expect_equal("chunk request status", request.status, 0u);
  ok &= expect_equal("chunk request columns", request.column_count, 9u);
  ok &= expect_equal("chunk request blocks", request.block_count, 2u);
  ok &= expect_equal("chunk request first column version", columns[0].version,
                     1u);
  ok &= expect_equal("chunk request first column x", columns[0].chunk_x, -1);
  ok &= expect_equal("chunk request center column block count",
                     columns[4].block_count, 1u);
  ok &=
      expect_equal("chunk request first block version", blocks[0].version, 1u);
  ok &= expect_equal("chunk request east block", blocks[1].block, uint16_t{6});
  return ok;
}

} // namespace

bool validate_chunk_request_frame() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 5));
  store.set_block(edit(32, 1, 0, 6));

  std::vector<octaryn_chunk_column_snapshot_column> columns(9u);
  std::vector<octaryn_chunk_column_snapshot_block> blocks(2u);
  octaryn_chunk_column_request_frame request{};
  reset_request(request, columns, blocks);

  bool ok = validate_successful_request(store, request, columns, blocks);

  request.column_capacity = 1u;
  request.block_capacity = static_cast<uint32_t>(blocks.size());
  ok &= expect_equal(
      "chunk request column capacity result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -3);
  ok &=
      expect_equal("chunk request required columns", request.column_count, 9u);
  ok &= expect_equal("chunk request capacity status", request.status, 3u);

  request.column_capacity = static_cast<uint32_t>(columns.size());
  request.block_capacity = 1u;
  ok &= expect_equal(
      "chunk request block capacity result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -4);
  ok &= expect_equal("chunk request required blocks", request.block_count, 2u);
  ok &= expect_equal("chunk request block capacity status", request.status, 4u);

  request.radius = 33u;
  request.block_capacity = static_cast<uint32_t>(blocks.size());
  request.column_capacity = static_cast<uint32_t>(columns.size());
  ok &= expect_equal(
      "chunk request radius result",
      octaryn_server_chunk_stream_request_columns(&store, &request), -2);
  ok &= expect_equal("chunk request radius status", request.status, 2u);

  ok &= expect_equal(
      "chunk request unavailable result",
      octaryn_server_chunk_stream_write_request_result(&request, 0u, 0u, 5u),
      -5);
  ok &= expect_equal("chunk request unavailable status", request.status, 5u);

  reset_request(request, columns, blocks);
  ok &= expect_equal(
      "chunk request unavailable gate result",
      octaryn_server_chunk_stream_request_columns_if_available(&store, 0u,
                                                               &request),
      -5);
  ok &= expect_equal("chunk request unavailable gate status", request.status,
                     5u);

  ok &= expect_equal(
      "chunk request available gate result",
      octaryn_server_chunk_stream_request_columns_if_available(&store, 1u,
                                                               &request),
      0);
  ok &= expect_equal("chunk request available gate columns",
                     request.column_count, 9u);

  octaryn_chunk_column_request_frame invalid_request{};
  ok &= expect_equal("chunk request invalid result writer",
                     octaryn_server_chunk_stream_write_request_result(
                         &invalid_request, 0u, 0u, 5u),
                     -1);
  return ok;
}
