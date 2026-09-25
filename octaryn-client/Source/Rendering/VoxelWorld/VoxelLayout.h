#pragma once

#include <cstdint>

namespace octaryn::client::voxel {

inline constexpr std::int32_t ChunkWidthBlocks = 32;
inline constexpr std::int32_t ChunkVoxelCount =
    ChunkWidthBlocks * ChunkWidthBlocks * ChunkWidthBlocks;
inline constexpr std::int32_t ColumnHeightChunks = 32;
inline constexpr std::int32_t WorldHeightBlocks =
    ChunkWidthBlocks * ColumnHeightChunks;
inline constexpr std::int32_t ColumnVoxelSlots =
    ChunkWidthBlocks * WorldHeightBlocks * ChunkWidthBlocks;
inline constexpr std::int32_t RenderDistanceMinChunks = 4;
inline constexpr std::int32_t RenderDistanceMaxChunks = 128;
inline constexpr std::int32_t RenderDistanceStepChunks = 4;
inline constexpr std::int32_t RenderDistanceOptionCount =
    (RenderDistanceMaxChunks - RenderDistanceMinChunks) /
        RenderDistanceStepChunks +
    1;
inline constexpr std::int32_t ColumnRangeWidth =
    RenderDistanceMaxChunks * 2 + 1;
inline constexpr std::int32_t ColumnRangeCount =
    ColumnRangeWidth * ColumnRangeWidth;

constexpr bool is_valid_render_distance(std::int32_t distance) noexcept
{
    return distance >= RenderDistanceMinChunks &&
           distance <= RenderDistanceMaxChunks &&
           ((distance - RenderDistanceMinChunks) %
            RenderDistanceStepChunks) == 0;
}

} // namespace octaryn::client::voxel
