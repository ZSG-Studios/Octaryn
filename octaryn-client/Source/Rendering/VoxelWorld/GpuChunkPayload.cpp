#include "GpuChunkPayload.h"

namespace octaryn::client::voxel {
namespace {

GpuChunkHeader make_header(std::uint32_t column_index, std::uint32_t chunk_y,
                           std::uint32_t flags) noexcept {
  return {column_index,
          chunk_y,
          0u,
          0u,
          flags,
          0u,
          0u,
          1u};
}

} // namespace

static_assert(sizeof(GpuChunkHeader) == 32);
static_assert(alignof(GpuChunkHeader) == alignof(std::uint32_t));
static_assert(sizeof(GpuChunkPaletteEntry) == 16);

GpuChunkPayload make_empty_gpu_chunk(std::uint32_t column_index,
                                     std::uint32_t chunk_y) noexcept {
  return {make_header(column_index, chunk_y, ChunkPaletteEmpty), {}, {}, 0u,
          0u};
}

GpuChunkPayload make_uniform_gpu_chunk(std::uint32_t column_index,
                                       std::uint32_t chunk_y,
                                       std::uint32_t block_id) {
  GpuChunkPayload chunk{make_header(column_index, chunk_y,
                                    ChunkPaletteUniform),
                        {GpuChunkPaletteEntry{block_id, block_id, 0u, 0u}},
                        {},
                        1u,
                        0u};
  chunk.header.uniform_block_id = block_id;
  return chunk;
}

GpuChunkPayload make_mixed_gpu_chunk(std::uint32_t column_index,
                                     std::uint32_t chunk_y,
                                     const GpuChunkPaletteEntry *palette,
                                     std::uint32_t palette_count,
                                     const std::uint8_t *payload,
                                     std::uint32_t payload_size_bytes) {
  GpuChunkPayload chunk{
      make_header(column_index, chunk_y, ChunkPaletteMixed),
      {},
      {},
      palette_count,
      choose_bits_per_voxel(static_cast<std::uint16_t>(palette_count))};
  if (palette != nullptr && palette_count > 0u) {
    chunk.palette.assign(palette, palette + palette_count);
  }
  if (payload != nullptr && payload_size_bytes > 0u) {
    chunk.payload.assign(payload, payload + payload_size_bytes);
  }
  return chunk;
}

bool gpu_chunk_payload_valid(const GpuChunkPayload &chunk) noexcept {
  if (chunk.header.neighbor_mask != 0u || chunk.header.dirty_revision == 0u) {
    return false;
  }
  if (chunk.palette_count > 65535u) {
    return false;
  }
  const ChunkPaletteHeader palette_header{
      static_cast<std::uint16_t>(chunk.palette_count),
      static_cast<std::uint8_t>(chunk.bits_per_voxel),
      static_cast<std::uint8_t>(chunk.header.flags)};
  if (!chunk_palette_header_valid(palette_header)) {
    return false;
  }
  const auto expected_payload =
      packed_voxel_payload_bytes(static_cast<std::uint8_t>(
          chunk.bits_per_voxel));
  if ((chunk.header.flags & ChunkPaletteMixed) != 0u) {
    return chunk.payload.size() == expected_payload &&
           chunk.palette.size() == chunk.palette_count &&
           chunk.header.uniform_block_id == 0u;
  }
  if ((chunk.header.flags & ChunkPaletteUniform) != 0u) {
    return chunk.payload.empty() && chunk.palette.size() == 1u &&
           chunk.header.uniform_block_id == chunk.palette.front().block_id;
  }
  return chunk.payload.empty() && chunk.palette.empty() &&
         chunk.header.uniform_block_id == 0u;
}

} // namespace octaryn::client::voxel
