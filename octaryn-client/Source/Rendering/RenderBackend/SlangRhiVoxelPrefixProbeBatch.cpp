#include "SlangRhiVoxelPrefixProbeBatch.h"

#include "VoxelLayout.h"

#include <array>

namespace octaryn::client::rendering {
namespace {

using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::GpuChunkPayload;

constexpr std::uint32_t CheckerboardChunk = 2u;
constexpr std::uint32_t MixedChunk = 3u;

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  return x + W * (y + W * z);
}

std::vector<std::uint8_t> build_payload_bytes(std::uint32_t chunk_kind) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  const std::uint32_t payload_size =
      octaryn::client::voxel::packed_voxel_payload_bytes(2u);
  std::vector<std::uint8_t> bytes(payload_size, 0u);
  for (std::uint32_t z = 0; z < W; ++z) {
    for (std::uint32_t y = 0; y < W; ++y) {
      for (std::uint32_t x = 0; x < W; ++x) {
        const std::uint32_t palette_index =
            chunk_kind == CheckerboardChunk ? ((x + y + z) & 1u) : (x & 3u);
        const std::uint32_t bit_offset = voxel_index(x, y, z) * 2u;
        bytes[bit_offset >> 3u] |=
            static_cast<std::uint8_t>(palette_index << (bit_offset & 7u));
      }
    }
  }
  return bytes;
}

GpuChunkPayload make_checkerboard_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{37u, 370u, 1u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
  };
  const auto payload = build_payload_bytes(CheckerboardChunk);
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      CheckerboardChunk, 0u, entries.data(),
      static_cast<std::uint32_t>(entries.size()), payload.data(),
      static_cast<std::uint32_t>(payload.size()));
}

GpuChunkPayload make_mixed_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{20u, 200u, 1u, 0u},
      GpuChunkPaletteEntry{21u, 210u, 1u, 0u},
      GpuChunkPaletteEntry{22u, 220u, 1u, 0u},
      GpuChunkPaletteEntry{23u, 230u, 1u, 0u},
  };
  const auto payload = build_payload_bytes(MixedChunk);
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      MixedChunk, 0u, entries.data(),
      static_cast<std::uint32_t>(entries.size()), payload.data(),
      static_cast<std::uint32_t>(payload.size()));
}

bool append_chunk(SlangRhiVoxelPrefixProbeBatch &batch,
                  GpuChunkPayload chunk) {
  if (!octaryn::client::voxel::gpu_chunk_payload_valid(chunk)) { return false; }
  chunk.header.palette_offset =
      static_cast<std::uint32_t>(batch.palette_entries.size());
  chunk.header.voxel_data_offset =
      static_cast<std::uint32_t>(batch.payload_bytes.size());
  batch.headers.push_back(chunk.header);
  batch.palette_entries.insert(batch.palette_entries.end(),
                               chunk.palette.begin(), chunk.palette.end());
  batch.payload_bytes.insert(batch.payload_bytes.end(), chunk.payload.begin(),
                             chunk.payload.end());
  return true;
}

} // namespace

SlangRhiVoxelPrefixProbeBatch build_slang_rhi_voxel_prefix_probe_batch() {
  SlangRhiVoxelPrefixProbeBatch batch;
  append_chunk(batch, octaryn::client::voxel::make_empty_gpu_chunk(0u, 0u));
  append_chunk(batch,
               octaryn::client::voxel::make_uniform_gpu_chunk(1u, 0u, 42u));
  append_chunk(batch, make_checkerboard_payload());
  append_chunk(batch, make_mixed_payload());
  return batch;
}

std::vector<std::uint8_t> make_slang_rhi_voxel_prefix_payload_word_bytes(
    const std::vector<std::uint8_t> &payload) {
  std::vector<std::uint8_t> bytes = payload;
  while ((bytes.size() % sizeof(std::uint32_t)) != 0u) {
    bytes.push_back(0u);
  }
  return bytes;
}

} // namespace octaryn::client::rendering
