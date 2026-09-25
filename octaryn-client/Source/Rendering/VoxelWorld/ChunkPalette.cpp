#include "ChunkPalette.h"

#include "VoxelLayout.h"

namespace octaryn::client::voxel {

std::uint8_t choose_bits_per_voxel(std::uint16_t palette_count) noexcept
{
    if (palette_count == 0u || palette_count == 1u) {
        return 0u;
    }
    if (palette_count <= 2u) {
        return 1u;
    }
    if (palette_count <= 4u) {
        return 2u;
    }
    if (palette_count <= 16u) {
        return 4u;
    }
    if (palette_count <= 256u) {
        return 8u;
    }
    return 16u;
}

bool chunk_palette_header_valid(const ChunkPaletteHeader& header) noexcept
{
    const auto expected_bits = choose_bits_per_voxel(header.palette_count);
    if (header.bits_per_voxel != expected_bits) {
        return false;
    }
    if ((header.flags & ChunkPaletteEmpty) != 0u) {
        return header.palette_count == 0u && header.bits_per_voxel == 0u;
    }
    if ((header.flags & ChunkPaletteUniform) != 0u) {
        return header.palette_count == 1u && header.bits_per_voxel == 0u;
    }
    if ((header.flags & ChunkPaletteMixed) != 0u) {
        return header.palette_count > 1u && header.bits_per_voxel != 0u;
    }
    return false;
}

std::uint32_t packed_voxel_payload_bytes(std::uint8_t bits_per_voxel) noexcept
{
    if (bits_per_voxel == 0u) {
        return 0u;
    }
    return static_cast<std::uint32_t>(
        (ChunkVoxelCount * bits_per_voxel + 7) / 8);
}

} // namespace octaryn::client::voxel
