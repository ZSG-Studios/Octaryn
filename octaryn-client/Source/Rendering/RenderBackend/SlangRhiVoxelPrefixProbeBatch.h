#pragma once

#include "GpuChunkPayload.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::rendering {

struct SlangRhiVoxelPrefixProbeBatch {
  std::vector<octaryn::client::voxel::GpuChunkHeader> headers;
  std::vector<octaryn::client::voxel::GpuChunkPaletteEntry> palette_entries;
  std::vector<std::uint8_t> payload_bytes;
};

SlangRhiVoxelPrefixProbeBatch build_slang_rhi_voxel_prefix_probe_batch();
std::vector<std::uint8_t> make_slang_rhi_voxel_prefix_payload_word_bytes(
    const std::vector<std::uint8_t> &payload);

} // namespace octaryn::client::rendering
