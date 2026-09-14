#pragma once

#include <cstdint>

namespace octaryn::client::voxel {

enum ChunkPaletteFlags : std::uint8_t {
    ChunkPaletteEmpty = 1u << 0u,
    ChunkPaletteUniform = 1u << 1u,
    ChunkPaletteMixed = 1u << 2u,
    ChunkPaletteDirty = 1u << 3u,
    ChunkPaletteGpuResident = 1u << 4u,
};

struct ChunkPaletteHeader {
    std::uint16_t palette_count;
    std::uint8_t bits_per_voxel;
    std::uint8_t flags;
};

struct ChunkPaletteEntry {
    std::uint32_t global_block_id;
};

std::uint8_t choose_bits_per_voxel(std::uint16_t palette_count) noexcept;
bool chunk_palette_header_valid(const ChunkPaletteHeader& header) noexcept;
std::uint32_t packed_voxel_payload_bytes(std::uint8_t bits_per_voxel) noexcept;

} // namespace octaryn::client::voxel
