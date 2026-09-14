#pragma once

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-gfx.h>
#endif

#include <cstdint>

namespace octaryn::client::rendering::detail {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)

struct SlangRhiVoxelRasterPassGraphPipelines {
  gfx::IPipelineState *face;
  gfx::IPipelineState *greedy;
  gfx::IPipelineState *prefix;
  gfx::IPipelineState *emit;
  gfx::IPipelineState *indirect;
};

struct SlangRhiVoxelRasterPassGraphViews {
  gfx::IResourceView *headers;
  gfx::IResourceView *palette;
  gfx::IResourceView *payload;
  gfx::IResourceView *mask_uav;
  gfx::IResourceView *mask_srv;
  gfx::IResourceView *face_counts;
  gfx::IResourceView *quad_count_uav;
  gfx::IResourceView *quad_count_srv;
  gfx::IResourceView *material_sums;
  gfx::IResourceView *offset_uav;
  gfx::IResourceView *offset_srv;
  gfx::IResourceView *total;
  gfx::IResourceView *quad_uav;
  gfx::IResourceView *quad_srv;
  gfx::IResourceView *emit_count_uav;
  gfx::IResourceView *emit_count_srv;
  gfx::IResourceView *emit_sums;
  gfx::IResourceView *emit_checksums;
  gfx::IResourceView *commands;
  gfx::IResourceView *draw_count;
  gfx::IResourceView *empty_count;
  gfx::IResourceView *instances;
  gfx::IResourceView *first_quad_checksums;
};

struct SlangRhiVoxelRasterPassGraphBuffers {
  gfx::IBufferResource *masks;
  gfx::IBufferResource *quad_counts;
  gfx::IBufferResource *offsets;
  gfx::IBufferResource *quads;
  gfx::IBufferResource *emit_counts;
};

const char *dispatch_slang_rhi_voxel_raster_pass_graph(
    gfx::IComputeCommandEncoder *encoder,
    const SlangRhiVoxelRasterPassGraphPipelines &pipelines,
    const SlangRhiVoxelRasterPassGraphViews &views,
    const SlangRhiVoxelRasterPassGraphBuffers &buffers, int chunk_count);

#endif

} // namespace octaryn::client::rendering::detail
