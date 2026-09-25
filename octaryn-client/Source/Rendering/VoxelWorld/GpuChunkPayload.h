#pragma once

#include "ChunkPalette.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::voxel {

struct GpuChunkHeader {
  std::uint32_t column_index;
  std::uint32_t chunk_y;
  std::uint32_t palette_offset;
  std::uint32_t voxel_data_offset;
  std::uint32_t flags;
  std::uint32_t uniform_block_id;
  std::uint32_t neighbor_mask;
  std::uint32_t dirty_revision;
};

struct GpuChunkPaletteEntry {
  std::uint32_t block_id;
  std::uint32_t material_id;
  std::uint32_t block_flags;
  std::uint32_t material_flags;
};

struct GpuChunkPayload {
  GpuChunkHeader header;
  std::vector<GpuChunkPaletteEntry> palette;
  std::vector<std::uint8_t> payload;
  std::uint32_t palette_count;
  std::uint32_t bits_per_voxel;
};

GpuChunkPayload make_empty_gpu_chunk(std::uint32_t column_index,
                                     std::uint32_t chunk_y) noexcept;
GpuChunkPayload make_uniform_gpu_chunk(std::uint32_t column_index,
                                       std::uint32_t chunk_y,
                                       std::uint32_t block_id);
GpuChunkPayload make_mixed_gpu_chunk(std::uint32_t column_index,
                                     std::uint32_t chunk_y,
                                     const GpuChunkPaletteEntry *palette,
                                     std::uint32_t palette_count,
                                     const std::uint8_t *payload,
                                     std::uint32_t payload_size_bytes);
bool gpu_chunk_payload_valid(const GpuChunkPayload &chunk) noexcept;

} // namespace octaryn::client::voxel
