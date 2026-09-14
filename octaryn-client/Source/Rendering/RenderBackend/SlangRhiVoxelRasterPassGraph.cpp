#include "SlangRhiVoxelRasterPassGraph.h"

#include "SlangRhiVoxelRasterFrameGpu.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-gfx.h>
#endif

#include <array>

namespace octaryn::client::rendering::detail {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)

const char *dispatch_slang_rhi_voxel_raster_pass_graph(
    gfx::IComputeCommandEncoder *encoder,
    const SlangRhiVoxelRasterPassGraphPipelines &pipelines,
    const SlangRhiVoxelRasterPassGraphViews &views,
    const SlangRhiVoxelRasterPassGraphBuffers &buffers, int chunk_count) {
  const std::array<gfx::IResourceView *, 5> face_views = {
      views.headers, views.palette, views.payload, views.mask_uav,
      views.face_counts};
  const std::array<gfx::IResourceView *, 6> greedy_views = {
      views.headers,     views.palette,        views.payload,
      views.mask_srv,    views.quad_count_uav, views.material_sums};
  const std::array<gfx::IResourceView *, 3> prefix_views = {
      views.quad_count_srv, views.offset_uav, views.total};
  const std::array<gfx::IResourceView *, 10> emit_views = {
      views.headers,        views.palette,       views.payload,
      views.mask_srv,       views.quad_count_srv, views.offset_srv,
      views.quad_uav,       views.emit_count_uav, views.emit_sums,
      views.emit_checksums};
  const std::array<gfx::IResourceView *, 8> indirect_views = {
      views.quad_srv,   views.emit_count_srv, views.offset_srv,
      views.commands,   views.draw_count,     views.empty_count,
      views.instances,  views.first_quad_checksums};

  if (!bind_compute(encoder, pipelines.face, face_views) ||
      !ok(encoder->dispatchCompute(chunk_count, 1, 1))) {
    return "face_mask_dispatch_failed";
  }
  encoder->bufferBarrier(buffers.masks, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_compute(encoder, pipelines.greedy, greedy_views) ||
      !ok(encoder->dispatchCompute(chunk_count, 1, 1))) {
    return "greedy_count_dispatch_failed";
  }
  encoder->bufferBarrier(buffers.quad_counts,
                         gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_compute(encoder, pipelines.prefix, prefix_views) ||
      !ok(encoder->dispatchCompute(1, 1, 1))) {
    return "prefix_dispatch_failed";
  }
  encoder->bufferBarrier(buffers.offsets, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_compute(encoder, pipelines.emit, emit_views) ||
      !ok(encoder->dispatchCompute(chunk_count, 1, 1))) {
    return "packed_quad_emit_dispatch_failed";
  }
  encoder->bufferBarrier(buffers.quads, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  encoder->bufferBarrier(buffers.emit_counts,
                         gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_compute(encoder, pipelines.indirect, indirect_views) ||
      !ok(encoder->dispatchCompute(1, 1, 1))) {
    return "indirect_generation_dispatch_failed";
  }
  return nullptr;
}

#endif

} // namespace octaryn::client::rendering::detail
