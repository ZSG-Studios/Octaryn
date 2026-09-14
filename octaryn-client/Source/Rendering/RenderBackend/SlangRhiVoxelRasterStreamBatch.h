#pragma once

#include "GpuChunkPayload.h"
#include "RenderDistanceRing.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::rendering {

struct SlangRhiVoxelRasterStreamBatch {
  std::vector<octaryn::client::voxel::GpuChunkHeader> headers;
  std::vector<octaryn::client::voxel::GpuChunkPaletteEntry> palette_entries;
  std::vector<std::uint8_t> payload_bytes;
  std::vector<octaryn::client::voxel::ColumnCoord> column_coords;
  octaryn::client::voxel::ColumnCoord stream_center;
  std::uint32_t bounded_columns;
  std::uint32_t available_columns;
  bool live_stream_source;
};

SlangRhiVoxelRasterStreamBatch
build_slang_rhi_voxel_raster_stream_batch();

SlangRhiVoxelRasterStreamBatch
build_slang_rhi_voxel_raster_stream_batch_for_center(
    octaryn::client::voxel::ColumnCoord center);

std::vector<std::uint8_t> make_slang_rhi_voxel_raster_payload_word_bytes(
    const std::vector<std::uint8_t> &payload);

} // namespace octaryn::client::rendering
