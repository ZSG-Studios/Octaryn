#include "SlangRhiVoxelRasterResources.h"

#include "PackedVoxelQuad.h"
#include "SlangRhiVoxelRasterFrameGpu.h"
#include "VoxelIndirect.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace octaryn::client::rendering::detail {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

using octaryn::client::voxel::DrawIndexedIndirectCommand;
using octaryn::client::voxel::GpuChunkHeader;
using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::PackedVoxelQuad16;

struct SlangRhiVoxelRasterResourceSizes {
  std::size_t headers;
  std::size_t palette;
  std::size_t payload;
  std::size_t masks;
  std::size_t chunk_counters;
  std::size_t quads;
  std::size_t commands;
};

std::uint64_t input_upload_bytes(const SlangRhiVoxelRasterResourceSizes &sizes) {
  return static_cast<std::uint64_t>(sizes.headers + sizes.palette +
                                    sizes.payload);
}

std::uint64_t retained_buffer_bytes(
    const SlangRhiVoxelRasterResourceSizes &sizes) {
  constexpr std::size_t scalar_counter_count = 5u;
  constexpr std::size_t chunk_counter_buffer_count = 7u;
  constexpr std::size_t index_bytes = 6u * sizeof(std::uint16_t);
  return static_cast<std::uint64_t>(
      sizes.headers + sizes.palette + sizes.payload + sizes.masks +
      sizes.quads + sizes.commands +
      chunk_counter_buffer_count * sizes.chunk_counters +
      scalar_counter_count * sizeof(std::uint32_t) + index_bytes);
}

SlangRhiVoxelRasterResourceSizes
measure_resources(const SlangRhiVoxelRasterStreamBatch &batch,
                  std::size_t payload_bytes) {
  return {batch.headers.size() * sizeof(GpuChunkHeader),
          batch.palette_entries.size() * sizeof(GpuChunkPaletteEntry),
          payload_bytes,
          batch.headers.size() * FaceMaskValuesPerChunk *
              sizeof(std::uint32_t),
          batch.headers.size() * sizeof(std::uint32_t),
          ExpectedTotalQuads * sizeof(PackedVoxelQuad16),
          batch.headers.size() * sizeof(DrawIndexedIndirectCommand)};
}

bool create_raster_buffers(gfx::IDevice *device,
                           const SlangRhiVoxelRasterStreamBatch &batch,
                           const std::vector<std::uint8_t> &payload_words,
                           const SlangRhiVoxelRasterResourceSizes &sizes,
                           SlangRhiVoxelRasterResources &resources) {
  const std::vector<std::uint8_t> header_bytes = serialize_values(batch.headers);
  const std::vector<std::uint8_t> palette_bytes =
      serialize_values(batch.palette_entries);
  const std::uint32_t zero = 0u;
  const std::array<std::uint16_t, 6> indices = {0u, 1u, 2u, 3u, 4u, 5u};
  const gfx::ResourceStateSet read_state(
      gfx::ResourceState::ShaderResource, gfx::ResourceState::CopyDestination);
  const gfx::ResourceStateSet uav_state(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource,
      gfx::ResourceState::IndirectArgument, gfx::ResourceState::CopySource);
  return create_buffer(device, sizes.headers, sizeof(GpuChunkHeader),
                       gfx::ResourceState::ShaderResource, read_state,
                       header_bytes.data(), resources.headers) &&
         create_buffer(device, sizes.palette, sizeof(GpuChunkPaletteEntry),
                       gfx::ResourceState::ShaderResource, read_state,
                       palette_bytes.data(), resources.palette) &&
         create_buffer(device, sizes.payload, sizeof(std::uint32_t),
                       gfx::ResourceState::ShaderResource, read_state,
                       payload_words.data(), resources.payload) &&
         create_buffer(device, sizes.masks, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.masks) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.face_counts) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.quad_counts) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.material_sums) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.offsets) &&
         create_buffer(device, sizeof(std::uint32_t), sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, &zero,
                       resources.total) &&
         create_buffer(device, sizes.quads, sizeof(PackedVoxelQuad16),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.quads) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.emit_counts) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.emit_sums) &&
         create_buffer(device, sizes.commands,
                       sizeof(DrawIndexedIndirectCommand),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.commands) &&
         create_buffer(device, sizeof(std::uint32_t), sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, &zero,
                       resources.draw_count) &&
         create_buffer(device, sizeof(std::uint32_t), sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, &zero,
                       resources.empty_count) &&
         create_buffer(device, sizeof(std::uint32_t), sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, &zero,
                       resources.instances) &&
         create_buffer(device, sizes.chunk_counters, sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, nullptr,
                       resources.checksums) &&
         create_buffer(device, sizeof(std::uint32_t), sizeof(std::uint32_t),
                       gfx::ResourceState::UnorderedAccess, uav_state, &zero,
                       resources.first_quad_checksums) &&
         create_buffer(device, sizeof(indices), sizeof(std::uint16_t),
                       gfx::ResourceState::IndexBuffer,
                       gfx::ResourceStateSet(gfx::ResourceState::IndexBuffer),
                       indices.data(), resources.index_buffer);
}

bool create_raster_views(gfx::IDevice *device,
                         const SlangRhiVoxelRasterResourceSizes &sizes,
                         SlangRhiVoxelRasterResources &resources) {
  return create_view(device, resources.headers,
                     gfx::IResourceView::Type::ShaderResource, sizes.headers,
                     resources.header_view) &&
         create_view(device, resources.palette,
                     gfx::IResourceView::Type::ShaderResource, sizes.palette,
                     resources.palette_view) &&
         create_view(device, resources.payload,
                     gfx::IResourceView::Type::ShaderResource, sizes.payload,
                     resources.payload_view) &&
         create_view(device, resources.masks,
                     gfx::IResourceView::Type::UnorderedAccess, sizes.masks,
                     resources.mask_uav_view) &&
         create_view(device, resources.face_counts,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.face_count_view) &&
         create_view(device, resources.masks,
                     gfx::IResourceView::Type::ShaderResource, sizes.masks,
                     resources.mask_srv_view) &&
         create_view(device, resources.quad_counts,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.quad_count_uav_view) &&
         create_view(device, resources.material_sums,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.material_view) &&
         create_view(device, resources.quad_counts,
                     gfx::IResourceView::Type::ShaderResource,
                     sizes.chunk_counters, resources.quad_count_srv_view) &&
         create_view(device, resources.offsets,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.offset_uav_view) &&
         create_view(device, resources.total,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizeof(std::uint32_t), resources.total_view) &&
         create_view(device, resources.offsets,
                     gfx::IResourceView::Type::ShaderResource,
                     sizes.chunk_counters, resources.offset_srv_view) &&
         create_view(device, resources.quads,
                     gfx::IResourceView::Type::UnorderedAccess, sizes.quads,
                     resources.quad_uav_view) &&
         create_view(device, resources.emit_counts,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.emit_count_uav_view) &&
         create_view(device, resources.emit_sums,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.emit_sum_view) &&
         create_view(device, resources.quads,
                     gfx::IResourceView::Type::ShaderResource, sizes.quads,
                     resources.quad_srv_view) &&
         create_view(device, resources.emit_counts,
                     gfx::IResourceView::Type::ShaderResource,
                     sizes.chunk_counters, resources.emit_count_srv_view) &&
         create_view(device, resources.commands,
                     gfx::IResourceView::Type::UnorderedAccess, sizes.commands,
                     resources.command_view) &&
         create_view(device, resources.draw_count,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizeof(std::uint32_t), resources.draw_count_view) &&
         create_view(device, resources.empty_count,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizeof(std::uint32_t), resources.empty_count_view) &&
         create_view(device, resources.instances,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizeof(std::uint32_t), resources.instance_view) &&
         create_view(device, resources.checksums,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizes.chunk_counters, resources.checksum_view) &&
         create_view(device, resources.first_quad_checksums,
                     gfx::IResourceView::Type::UnorderedAccess,
                     sizeof(std::uint32_t),
                     resources.first_quad_checksum_view);
}

} // namespace

bool create_slang_rhi_voxel_raster_resources(
    gfx::IDevice *device, const SlangRhiVoxelRasterStreamBatch &batch,
    SlangRhiVoxelRasterResources &resources) {
  const std::vector<std::uint8_t> payload_words =
      make_slang_rhi_voxel_raster_payload_word_bytes(batch.payload_bytes);
  const SlangRhiVoxelRasterResourceSizes sizes =
      measure_resources(batch, payload_words.size());
  if (!create_raster_buffers(device, batch, payload_words, sizes, resources) ||
      !create_raster_views(device, sizes, resources)) {
    return false;
  }
  resources.retained_gpu_bytes = retained_buffer_bytes(sizes);
  resources.upload_staging_bytes = input_upload_bytes(sizes);
  return true;
}

bool upload_slang_rhi_voxel_raster_input_resources(
    gfx::IResourceCommandEncoder *encoder,
    const SlangRhiVoxelRasterStreamBatch &batch,
    SlangRhiVoxelRasterResources &resources) {
  if (encoder == nullptr) { return false; }
  const std::vector<std::uint8_t> header_bytes = serialize_values(batch.headers);
  const std::vector<std::uint8_t> palette_bytes =
      serialize_values(batch.palette_entries);
  const std::vector<std::uint8_t> payload_words =
      make_slang_rhi_voxel_raster_payload_word_bytes(batch.payload_bytes);
  const SlangRhiVoxelRasterResourceSizes sizes =
      measure_resources(batch, payload_words.size());
  encoder->bufferBarrier(resources.headers, gfx::ResourceState::ShaderResource,
                         gfx::ResourceState::CopyDestination);
  encoder->uploadBufferData(resources.headers, 0, header_bytes.size(),
                            const_cast<std::uint8_t *>(header_bytes.data()));
  encoder->bufferBarrier(resources.headers, gfx::ResourceState::CopyDestination,
                         gfx::ResourceState::ShaderResource);
  encoder->bufferBarrier(resources.palette, gfx::ResourceState::ShaderResource,
                         gfx::ResourceState::CopyDestination);
  encoder->uploadBufferData(resources.palette, 0, palette_bytes.size(),
                            const_cast<std::uint8_t *>(palette_bytes.data()));
  encoder->bufferBarrier(resources.palette, gfx::ResourceState::CopyDestination,
                         gfx::ResourceState::ShaderResource);
  encoder->bufferBarrier(resources.payload, gfx::ResourceState::ShaderResource,
                         gfx::ResourceState::CopyDestination);
  encoder->uploadBufferData(resources.payload, 0, payload_words.size(),
                            const_cast<std::uint8_t *>(payload_words.data()));
  encoder->bufferBarrier(resources.payload, gfx::ResourceState::CopyDestination,
                         gfx::ResourceState::ShaderResource);
  resources.upload_staging_bytes = input_upload_bytes(sizes);
  return true;
}

SlangRhiVoxelRasterPassGraphViews
slang_rhi_voxel_raster_pass_graph_views(
    const SlangRhiVoxelRasterResources &resources) {
  return {resources.header_view,
          resources.palette_view,
          resources.payload_view,
          resources.mask_uav_view,
          resources.mask_srv_view,
          resources.face_count_view,
          resources.quad_count_uav_view,
          resources.quad_count_srv_view,
          resources.material_view,
          resources.offset_uav_view,
          resources.offset_srv_view,
          resources.total_view,
          resources.quad_uav_view,
          resources.quad_srv_view,
          resources.emit_count_uav_view,
          resources.emit_count_srv_view,
          resources.emit_sum_view,
          resources.checksum_view,
          resources.command_view,
          resources.draw_count_view,
          resources.empty_count_view,
          resources.instance_view,
          resources.first_quad_checksum_view};
}

SlangRhiVoxelRasterPassGraphBuffers
slang_rhi_voxel_raster_pass_graph_buffers(
    const SlangRhiVoxelRasterResources &resources) {
  return {resources.masks, resources.quad_counts, resources.offsets,
          resources.quads, resources.emit_counts};
}

#endif

} // namespace octaryn::client::rendering::detail
