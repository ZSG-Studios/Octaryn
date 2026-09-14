#include "SlangRhiVoxelRasterStreamBatch.h"

#include "ColumnStreaming.h"
#include "VoxelLayout.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace octaryn::client::rendering {
namespace {

using octaryn::client::voxel::ColumnCoord;
using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::GpuChunkPayload;

struct StreamBlockRecord {
  std::int32_t x;
  std::int32_t y;
  std::int32_t z;
  std::uint16_t block;
};

struct StreamColumnRecord {
  ColumnCoord coord;
  std::int32_t origin_x;
  std::int32_t origin_z;
  std::uint32_t block_offset;
  std::uint32_t block_count;
};

struct StreamRecordBatch {
  std::vector<StreamColumnRecord> columns;
  std::vector<StreamBlockRecord> blocks;
  ColumnCoord center;
};

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  return x + W * (y + W * z);
}

template <typename T> bool read_value(std::ifstream &input, T &value) {
  input.read(reinterpret_cast<char *>(&value), sizeof(T));
  return static_cast<bool>(input);
}

std::filesystem::path binary_snapshot_path_for(const char *stream_path) {
  return std::filesystem::path{std::string(stream_path) + ".bin"};
}

const char *live_stream_path() {
  const char *path = std::getenv("OCTARYN_CLIENT_CHUNK_STREAM_PATH");
  if (path != nullptr && path[0] != '\0') { return path; }
  path = std::getenv("OCTARYN_SERVER_CHUNK_STREAM_PATH");
  return path != nullptr && path[0] != '\0' ? path : nullptr;
}

bool read_snapshot_header(std::ifstream &input, StreamRecordBatch &stream,
                          std::uint32_t &column_count,
                          std::uint32_t &block_count) {
  constexpr char kMagic[8] = {'O', 'C', 'S', 'T', 'R', 'M', '0', '1'};
  char magic[8]{};
  std::uint32_t version = 0u;
  std::uint64_t ignored_u64 = 0u;
  std::uint32_t ignored_u32 = 0u;
  double ignored_double = 0.0;
  float ignored_float = 0.0f;
  input.read(magic, sizeof(magic));
  std::int32_t center_x = 0;
  std::int32_t center_z = 0;
  return input && std::memcmp(magic, kMagic, sizeof(kMagic)) == 0 &&
         read_value(input, version) && version == 1u &&
         read_value(input, ignored_u64) && read_value(input, center_x) &&
         read_value(input, center_z) && read_value(input, ignored_u32) &&
         read_value(input, ignored_u64) && read_value(input, ignored_u64) &&
         read_value(input, ignored_u32) && read_value(input, ignored_double) &&
         read_value(input, ignored_float) && read_value(input, ignored_float) &&
         read_value(input, ignored_float) && read_value(input, ignored_float) &&
         read_value(input, ignored_float) && read_value(input, ignored_float) &&
         read_value(input, ignored_float) && read_value(input, ignored_float) &&
         read_value(input, ignored_float) && read_value(input, ignored_u32) &&
         read_value(input, ignored_u32) && read_value(input, column_count) &&
         read_value(input, block_count) &&
         ((stream.center = ColumnCoord{center_x, center_z}), true);
}

bool read_stream_snapshot_records(StreamRecordBatch &stream) {
  const char *path = live_stream_path();
  if (path == nullptr) { return false; }
  std::ifstream input{binary_snapshot_path_for(path), std::ios::binary};
  std::uint32_t column_count = 0u;
  std::uint32_t block_count = 0u;
  if (!input || !read_snapshot_header(input, stream, column_count,
                                      block_count)) {
    return false;
  }

  std::vector<StreamColumnRecord> columns(column_count);
  for (auto &column : columns) {
    if (!read_value(input, column.coord.x) ||
        !read_value(input, column.coord.z) ||
        !read_value(input, column.origin_x) ||
        !read_value(input, column.origin_z) ||
        !read_value(input, column.block_offset) ||
        !read_value(input, column.block_count)) {
      return false;
    }
  }

  std::vector<StreamBlockRecord> blocks(block_count);
  for (auto &block : blocks) {
    if (!read_value(input, block.x) || !read_value(input, block.y) ||
        !read_value(input, block.z) || !read_value(input, block.block)) {
      return false;
    }
  }
  stream.columns = std::move(columns);
  stream.blocks = std::move(blocks);
  return true;
}

StreamRecordBatch build_fixture_stream_records(
    const std::vector<octaryn::client::voxel::ColumnRequest> &requests,
    std::uint32_t count, ColumnCoord center) {
  StreamRecordBatch stream{};
  stream.center = center;
  stream.columns.reserve(count);
  for (std::uint32_t i = 0u; i < count; ++i) {
    const auto coord = requests[i].coord;
    const auto origin_x = static_cast<std::int32_t>(
        coord.x * octaryn::client::voxel::ChunkWidthBlocks);
    const auto origin_z = static_cast<std::int32_t>(
        coord.z * octaryn::client::voxel::ChunkWidthBlocks);
    const auto offset = static_cast<std::uint32_t>(stream.blocks.size());
    if (i > 0u) {
      for (std::uint32_t j = 0u; j < i; ++j) {
        stream.blocks.push_back(StreamBlockRecord{
            origin_x + static_cast<std::int32_t>(i + j),
            static_cast<std::int32_t>(j), origin_z + static_cast<std::int32_t>(i),
            static_cast<std::uint16_t>(40u + i + j)});
      }
    }
    stream.columns.push_back(StreamColumnRecord{
        coord, origin_x, origin_z, offset,
        static_cast<std::uint32_t>(stream.blocks.size()) - offset});
  }
  return stream;
}

bool column_contains(const StreamColumnRecord &column, std::uint32_t chunk_y,
                     const StreamBlockRecord &block) {
  constexpr auto W =
      static_cast<std::int32_t>(octaryn::client::voxel::ChunkWidthBlocks);
  const auto origin_y = static_cast<std::int32_t>(chunk_y * W);
  return block.x >= column.origin_x && block.x < column.origin_x + W &&
         block.z >= column.origin_z && block.z < column.origin_z + W &&
         block.y >= origin_y && block.y < origin_y + W;
}

std::uint32_t chunk_y_for_column(const StreamColumnRecord &column,
                                 const std::vector<StreamBlockRecord> &blocks) {
  const auto end = std::min<std::uint32_t>(
      column.block_offset + column.block_count,
      static_cast<std::uint32_t>(blocks.size()));
  for (std::uint32_t i = column.block_offset; i < end; ++i) {
    const auto &block = blocks[i];
    if (block.block == 0u || !column_contains(column, 0u, block)) {
      continue;
    }
    return static_cast<std::uint32_t>(
        block.y / octaryn::client::voxel::ChunkWidthBlocks);
  }
  for (std::uint32_t i = column.block_offset; i < end; ++i) {
    const auto &block = blocks[i];
    if (block.block != 0u && block.y >= 0 &&
        block.y < octaryn::client::voxel::WorldHeightBlocks) {
      return static_cast<std::uint32_t>(
          block.y / octaryn::client::voxel::ChunkWidthBlocks);
    }
  }
  return 0u;
}

std::vector<std::uint32_t>
select_bounded_columns(const std::vector<StreamColumnRecord> &columns,
                       std::uint32_t max_columns) {
  std::vector<std::uint32_t> selected;
  selected.reserve(std::min<std::uint32_t>(
      max_columns, static_cast<std::uint32_t>(columns.size())));
  for (std::uint32_t i = 0u; i < columns.size() && selected.size() < max_columns;
       ++i) {
    if (columns[i].block_count > 0u) { selected.push_back(i); }
  }
  for (std::uint32_t i = 0u; i < columns.size() && selected.size() < max_columns;
       ++i) {
    if (columns[i].block_count == 0u) { selected.push_back(i); }
  }
  return selected;
}

std::uint32_t palette_index_for_block(
    std::array<GpuChunkPaletteEntry, 4> &entries, std::uint32_t block) {
  for (std::uint32_t i = 1u; i < entries.size(); ++i) {
    if (entries[i].block_id == block) { return i; }
  }
  for (std::uint32_t i = 1u; i < entries.size(); ++i) {
    if (entries[i].block_id == 0u) {
      entries[i] = GpuChunkPaletteEntry{block, block, 1u, 0u};
      return i;
    }
  }
  return 1u;
}

GpuChunkPayload make_stream_chunk(std::uint32_t column_index,
                                  const StreamColumnRecord &column,
                                  const std::vector<StreamBlockRecord> &blocks) {
  const std::uint32_t chunk_y = chunk_y_for_column(column, blocks);
  std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
  };
  const std::uint32_t payload_size =
      octaryn::client::voxel::packed_voxel_payload_bytes(2u);
  std::vector<std::uint8_t> payload(payload_size, 0u);
  std::uint32_t edited_blocks = 0u;
  const auto end = std::min<std::uint32_t>(
      column.block_offset + column.block_count,
      static_cast<std::uint32_t>(blocks.size()));
  for (std::uint32_t i = column.block_offset; i < end; ++i) {
    const auto &block = blocks[i];
    if (!column_contains(column, chunk_y, block) || block.block == 0u) {
      continue;
    }
    const auto lx = static_cast<std::uint32_t>(block.x - column.origin_x);
    const auto ly = static_cast<std::uint32_t>(
        block.y - static_cast<std::int32_t>(
                      chunk_y * octaryn::client::voxel::ChunkWidthBlocks));
    const auto lz = static_cast<std::uint32_t>(block.z - column.origin_z);
    const std::uint32_t palette_index =
        palette_index_for_block(entries, block.block);
    const std::uint32_t bit_offset = voxel_index(lx, ly, lz) * 2u;
    payload[bit_offset >> 3u] |=
        static_cast<std::uint8_t>(palette_index << (bit_offset & 7u));
    ++edited_blocks;
  }
  if (edited_blocks == 0u) {
    return octaryn::client::voxel::make_empty_gpu_chunk(column_index, chunk_y);
  }
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      column_index, chunk_y, entries.data(),
      static_cast<std::uint32_t>(entries.size()), payload.data(),
      static_cast<std::uint32_t>(payload.size()));
}

bool append_chunk(SlangRhiVoxelRasterStreamBatch &batch, GpuChunkPayload chunk,
                  ColumnCoord coord) {
  if (!octaryn::client::voxel::gpu_chunk_payload_valid(chunk)) { return false; }
  chunk.header.palette_offset =
      static_cast<std::uint32_t>(batch.palette_entries.size());
  chunk.header.voxel_data_offset =
      static_cast<std::uint32_t>(batch.payload_bytes.size());
  batch.headers.push_back(chunk.header);
  batch.column_coords.push_back(coord);
  batch.palette_entries.insert(batch.palette_entries.end(),
                               chunk.palette.begin(), chunk.palette.end());
  batch.payload_bytes.insert(batch.payload_bytes.end(), chunk.payload.begin(),
                             chunk.payload.end());
  return true;
}

} // namespace

SlangRhiVoxelRasterStreamBatch build_batch_from_stream_records(
    StreamRecordBatch stream, std::uint32_t max_columns, bool live_source) {
  SlangRhiVoxelRasterStreamBatch batch{};
  batch.stream_center = stream.center;
  batch.live_stream_source = live_source;
  batch.available_columns = static_cast<std::uint32_t>(stream.columns.size());
  const auto selected = select_bounded_columns(stream.columns, max_columns);
  for (std::uint32_t i = 0u; i < selected.size(); ++i) {
    const auto column_index = selected[i];
    if (!append_chunk(batch, make_stream_chunk(i, stream.columns[column_index],
                                               stream.blocks),
                      stream.columns[column_index].coord)) {
      return {};
    }
  }
  batch.bounded_columns = static_cast<std::uint32_t>(selected.size());
  return batch;
}

SlangRhiVoxelRasterStreamBatch
build_slang_rhi_voxel_raster_stream_batch_for_center(ColumnCoord center) {
  const auto budget = octaryn::client::voxel::default_streaming_budget();
  StreamRecordBatch live_stream{};
  if (read_stream_snapshot_records(live_stream)) {
    return build_batch_from_stream_records(live_stream,
                                           budget.max_generated_columns_accepted,
                                           true);
  }

  const auto requests = octaryn::client::voxel::build_required_columns(
      center, octaryn::client::voxel::RenderDistanceMinChunks);
  const std::uint32_t count =
      std::min<std::uint32_t>(budget.max_generated_columns_accepted,
                              static_cast<std::uint32_t>(requests.size()));
  return build_batch_from_stream_records(
      build_fixture_stream_records(requests, count, center), count, false);
}

SlangRhiVoxelRasterStreamBatch
build_slang_rhi_voxel_raster_stream_batch() {
  return build_slang_rhi_voxel_raster_stream_batch_for_center(
      ColumnCoord{0, 0});
}

std::vector<std::uint8_t> make_slang_rhi_voxel_raster_payload_word_bytes(
    const std::vector<std::uint8_t> &payload) {
  std::vector<std::uint8_t> bytes = payload;
  while ((bytes.size() % sizeof(std::uint32_t)) != 0u) {
    bytes.push_back(0u);
  }
  return bytes;
}

} // namespace octaryn::client::rendering
