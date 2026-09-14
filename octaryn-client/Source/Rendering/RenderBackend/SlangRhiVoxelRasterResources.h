#pragma once

#include "SlangRhiVoxelRasterPassGraph.h"
#include "SlangRhiVoxelRasterStreamBatch.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>
#endif

#include <cstdint>

namespace octaryn::client::rendering::detail {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)

struct SlangRhiVoxelRasterResources {
  Slang::ComPtr<gfx::IBufferResource> headers;
  Slang::ComPtr<gfx::IBufferResource> palette;
  Slang::ComPtr<gfx::IBufferResource> payload;
  Slang::ComPtr<gfx::IBufferResource> masks;
  Slang::ComPtr<gfx::IBufferResource> face_counts;
  Slang::ComPtr<gfx::IBufferResource> quad_counts;
  Slang::ComPtr<gfx::IBufferResource> material_sums;
  Slang::ComPtr<gfx::IBufferResource> offsets;
  Slang::ComPtr<gfx::IBufferResource> total;
  Slang::ComPtr<gfx::IBufferResource> quads;
  Slang::ComPtr<gfx::IBufferResource> emit_counts;
  Slang::ComPtr<gfx::IBufferResource> emit_sums;
  Slang::ComPtr<gfx::IBufferResource> commands;
  Slang::ComPtr<gfx::IBufferResource> draw_count;
  Slang::ComPtr<gfx::IBufferResource> empty_count;
  Slang::ComPtr<gfx::IBufferResource> instances;
  Slang::ComPtr<gfx::IBufferResource> checksums;
  Slang::ComPtr<gfx::IBufferResource> first_quad_checksums;
  Slang::ComPtr<gfx::IBufferResource> index_buffer;
  std::uint64_t retained_gpu_bytes;
  std::uint64_t upload_staging_bytes;

  Slang::ComPtr<gfx::IResourceView> header_view;
  Slang::ComPtr<gfx::IResourceView> palette_view;
  Slang::ComPtr<gfx::IResourceView> payload_view;
  Slang::ComPtr<gfx::IResourceView> mask_uav_view;
  Slang::ComPtr<gfx::IResourceView> mask_srv_view;
  Slang::ComPtr<gfx::IResourceView> face_count_view;
  Slang::ComPtr<gfx::IResourceView> quad_count_uav_view;
  Slang::ComPtr<gfx::IResourceView> quad_count_srv_view;
  Slang::ComPtr<gfx::IResourceView> material_view;
  Slang::ComPtr<gfx::IResourceView> offset_uav_view;
  Slang::ComPtr<gfx::IResourceView> offset_srv_view;
  Slang::ComPtr<gfx::IResourceView> total_view;
  Slang::ComPtr<gfx::IResourceView> quad_uav_view;
  Slang::ComPtr<gfx::IResourceView> quad_srv_view;
  Slang::ComPtr<gfx::IResourceView> emit_count_uav_view;
  Slang::ComPtr<gfx::IResourceView> emit_count_srv_view;
  Slang::ComPtr<gfx::IResourceView> emit_sum_view;
  Slang::ComPtr<gfx::IResourceView> command_view;
  Slang::ComPtr<gfx::IResourceView> draw_count_view;
  Slang::ComPtr<gfx::IResourceView> empty_count_view;
  Slang::ComPtr<gfx::IResourceView> instance_view;
  Slang::ComPtr<gfx::IResourceView> checksum_view;
  Slang::ComPtr<gfx::IResourceView> first_quad_checksum_view;
};

bool create_slang_rhi_voxel_raster_resources(
    gfx::IDevice *device, const SlangRhiVoxelRasterStreamBatch &batch,
    SlangRhiVoxelRasterResources &resources);

bool upload_slang_rhi_voxel_raster_input_resources(
    gfx::IResourceCommandEncoder *encoder,
    const SlangRhiVoxelRasterStreamBatch &batch,
    SlangRhiVoxelRasterResources &resources);

SlangRhiVoxelRasterPassGraphViews
slang_rhi_voxel_raster_pass_graph_views(
    const SlangRhiVoxelRasterResources &resources);

SlangRhiVoxelRasterPassGraphBuffers
slang_rhi_voxel_raster_pass_graph_buffers(
    const SlangRhiVoxelRasterResources &resources);

#endif

} // namespace octaryn::client::rendering::detail
