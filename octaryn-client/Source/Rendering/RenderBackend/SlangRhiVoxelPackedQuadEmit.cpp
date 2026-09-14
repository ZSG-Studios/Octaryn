#include "SlangComputePipeline.h"

#include "SlangRhiVoxelPackedQuadEmit.h"

#include "PackedVoxelQuad.h"
#include "SlangRhiVoxelFaceMaskExpected.h"
#include "SlangRhiVoxelPackedQuadValidation.h"
#include "SlangRhiVoxelPrefixProbeBatch.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

using octaryn::client::voxel::GpuChunkHeader;
using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::PackedVoxelQuad16;

constexpr const char *FaceMaskShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelFaceMaskProbe.slang";
constexpr const char *GreedyCountShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelGreedyCountProbe.slang";
constexpr const char *PrefixScanShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelPrefixScanProbe.slang";
constexpr const char *PackedEmitShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelPackedQuadEmitProbe.slang";
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount> ExpectedOffsets = {
    0u, 0u, 6u, 98310u};
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount> ExpectedCounts = {
    0u, 6u, 98304u, 130u};
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount>
    ExpectedMaterialSums = {0u, 252u, 36372480u, 27950u};
constexpr std::uint32_t ExpectedTotalQuads = 98440u;

bool succeeded(SlangResult result) { return SLANG_SUCCEEDED(result); }

void append_bytes(std::vector<std::uint8_t> &bytes, const void *source,
                  std::size_t byte_count) {
  if (byte_count == 0u) { return; }
  const auto *typed = static_cast<const std::uint8_t *>(source);
  bytes.insert(bytes.end(), typed, typed + byte_count);
}

template <typename T>
std::vector<std::uint8_t> serialize_values(const std::vector<T> &values) {
  std::vector<std::uint8_t> bytes;
  append_bytes(bytes, values.data(), values.size() * sizeof(T));
  return bytes;
}

bool create_buffer(gfx::IDevice *device, std::size_t byte_count,
                   std::size_t element_size, gfx::ResourceState state,
                   gfx::ResourceStateSet states, const void *initial_data,
                   Slang::ComPtr<gfx::IBufferResource> &buffer) {
  gfx::IBufferResource::Desc desc{};
  desc.type = gfx::IResource::Type::Buffer;
  desc.defaultState = state;
  desc.allowedStates = states;
  desc.memoryType = gfx::MemoryType::DeviceLocal;
  desc.sizeInBytes = byte_count;
  desc.elementSize = element_size;
  return succeeded(device->createBufferResource(desc, initial_data,
                                                buffer.writeRef())) &&
         buffer != nullptr;
}

bool create_view(gfx::IDevice *device, gfx::IBufferResource *buffer,
                 gfx::IResourceView::Type type, std::size_t byte_count,
                 Slang::ComPtr<gfx::IResourceView> &view) {
  gfx::IResourceView::Desc desc{};
  desc.type = type;
  desc.format = gfx::Format::Unknown;
  desc.bufferRange.offset = 0;
  desc.bufferRange.size = byte_count;
  return succeeded(
             device->createBufferView(buffer, nullptr, desc, view.writeRef())) &&
         view != nullptr;
}



bool set_resource(gfx::IShaderObject *object, gfx::GfxIndex index,
                  gfx::IResourceView *view) {
  return object != nullptr && succeeded(object->setResource({0, index, 0}, view));
}

template <std::size_t N>
bool bind_resources(gfx::IComputeCommandEncoder *encoder,
                    gfx::IPipelineState *pipeline,
                    const std::array<gfx::IResourceView *, N> &views) {
  Slang::ComPtr<gfx::IShaderObject> root;
  if (!succeeded(encoder->bindPipeline(pipeline, root.writeRef())) ||
      root == nullptr) {
    return false;
  }
  Slang::ComPtr<gfx::IShaderObject> entry_point;
  root->getEntryPoint(0, entry_point.writeRef());
  bool root_ok = true;
  bool entry_ok = true;
  for (std::size_t i = 0; i < views.size(); ++i) {
    const auto index = static_cast<gfx::GfxIndex>(i);
    root_ok &= set_resource(root, index, views[i]);
    entry_ok &= set_resource(entry_point, index, views[i]);
  }
  return root_ok || entry_ok;
}

template <typename T>
bool readback_values(gfx::IDevice *device, gfx::IBufferResource *buffer,
                     std::vector<T> &values) {
  Slang::ComPtr<ISlangBlob> readback;
  const std::size_t bytes = values.size() * sizeof(T);
  if (!succeeded(device->readBufferResource(buffer, 0, bytes,
                                            readback.writeRef())) ||
      readback == nullptr || readback->getBufferSize() != bytes) {
    return false;
  }
  std::memcpy(values.data(), readback->getBufferPointer(), bytes);
  return true;
}

SlangRhiVoxelPackedQuadEmitProbeResult make_result(const char *status) {
  return {true, false, false, false, false, false, false, false,
          false, false, false, false, false, false, false, 0u,
          0u,   0u,    0u,    0u,    0u,    0u,    0u,    0u,
          status};
}

} // namespace
#endif

SlangRhiVoxelPackedQuadEmitProbeResult
probe_slang_rhi_voxel_packed_quad_emit() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  auto probe = make_result("not_started");
  const auto batch = build_slang_rhi_voxel_prefix_probe_batch();
  const auto expected_masks = build_slang_rhi_voxel_face_mask_expected();
  if (batch.headers.size() != FaceMaskProbeChunkCount ||
      batch.palette_entries.size() != 9u || batch.payload_bytes.empty()) {
    probe.status = "packed_quad_emit_batch_invalid";
    return probe;
  }

  const auto header_bytes = serialize_values(batch.headers);
  const auto palette_bytes = serialize_values(batch.palette_entries);
  const auto payload_bytes =
      make_slang_rhi_voxel_prefix_payload_word_bytes(batch.payload_bytes);
  const std::size_t mask_bytes =
      expected_masks.masks.size() * sizeof(std::uint32_t);
  constexpr std::size_t CountBytes =
      FaceMaskProbeChunkCount * sizeof(std::uint32_t);
  constexpr std::size_t TotalBytes = sizeof(std::uint32_t);
  constexpr std::size_t QuadBytes =
      ExpectedTotalQuads * sizeof(PackedVoxelQuad16);

  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  Slang::ComPtr<gfx::IDevice> device;
  if (!succeeded(gfx::gfxCreateDevice(&device_desc, device.writeRef())) ||
      device == nullptr) {
    probe.status = "device_create_failed";
    return probe;
  }
  probe.device_created = true;

  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  if (!succeeded(device->createCommandQueue(queue_desc, queue.writeRef())) ||
      queue == nullptr) {
    probe.status = "queue_create_failed";
    return probe;
  }
  probe.queue_created = true;

  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 20;
  heap_desc.uavDescriptorCount = 20;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!succeeded(device->createTransientResourceHeap(heap_desc,
                                                     heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  Slang::ComPtr<gfx::IPipelineState> face_pipeline;
  Slang::ComPtr<gfx::IPipelineState> greedy_pipeline;
  Slang::ComPtr<gfx::IPipelineState> prefix_pipeline;
  Slang::ComPtr<gfx::IPipelineState> emit_pipeline;
  if (!create_slang_compute_pipeline(device, FaceMaskShaderPath, face_pipeline) ||
      !create_slang_compute_pipeline(device, GreedyCountShaderPath, greedy_pipeline) ||
      !create_slang_compute_pipeline(device, PrefixScanShaderPath, prefix_pipeline) ||
      !create_slang_compute_pipeline(device, PackedEmitShaderPath, emit_pipeline)) {
    probe.status = "compute_pipeline_create_failed";
    return probe;
  }
  probe.pipelines_created = true;

  const auto input_states =
      gfx::ResourceStateSet(gfx::ResourceState::ShaderResource);
  const auto mask_states = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource);
  const auto count_states = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource,
      gfx::ResourceState::CopySource);
  const auto output_states = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::CopySource);
  const auto offset_states = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource,
      gfx::ResourceState::CopySource);

  Slang::ComPtr<gfx::IBufferResource> header_buffer, palette_buffer;
  Slang::ComPtr<gfx::IBufferResource> payload_buffer, mask_buffer;
  Slang::ComPtr<gfx::IBufferResource> face_count_buffer, quad_count_buffer;
  Slang::ComPtr<gfx::IBufferResource> material_sum_buffer, offset_buffer;
  Slang::ComPtr<gfx::IBufferResource> total_buffer, quad_buffer;
  Slang::ComPtr<gfx::IBufferResource> emit_count_buffer, emit_sum_buffer;
  Slang::ComPtr<gfx::IBufferResource> checksum_buffer;
  if (!create_buffer(device, header_bytes.size(), sizeof(GpuChunkHeader),
                     gfx::ResourceState::ShaderResource, input_states,
                     header_bytes.data(), header_buffer) ||
      !create_buffer(device, palette_bytes.size(), sizeof(GpuChunkPaletteEntry),
                     gfx::ResourceState::ShaderResource, input_states,
                     palette_bytes.data(), palette_buffer) ||
      !create_buffer(device, payload_bytes.size(), sizeof(std::uint32_t),
                     gfx::ResourceState::ShaderResource, input_states,
                     payload_bytes.data(), payload_buffer) ||
      !create_buffer(device, mask_bytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, mask_states, nullptr,
                     mask_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     face_count_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, count_states, nullptr,
                     quad_count_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     material_sum_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, offset_states, nullptr,
                     offset_buffer) ||
      !create_buffer(device, TotalBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     total_buffer) ||
      !create_buffer(device, QuadBytes, sizeof(PackedVoxelQuad16),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     quad_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     emit_count_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     emit_sum_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     checksum_buffer)) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffers_created = true;

  std::array<Slang::ComPtr<gfx::IResourceView>, 16> views;
  if (!create_view(device, header_buffer,
                   gfx::IResourceView::Type::ShaderResource,
                   header_bytes.size(), views[0]) ||
      !create_view(device, palette_buffer,
                   gfx::IResourceView::Type::ShaderResource,
                   palette_bytes.size(), views[1]) ||
      !create_view(device, payload_buffer,
                   gfx::IResourceView::Type::ShaderResource,
                   payload_bytes.size(), views[2]) ||
      !create_view(device, mask_buffer, gfx::IResourceView::Type::UnorderedAccess,
                   mask_bytes, views[3]) ||
      !create_view(device, face_count_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[4]) ||
      !create_view(device, mask_buffer, gfx::IResourceView::Type::ShaderResource,
                   mask_bytes, views[5]) ||
      !create_view(device, quad_count_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[6]) ||
      !create_view(device, material_sum_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[7]) ||
      !create_view(device, quad_count_buffer,
                   gfx::IResourceView::Type::ShaderResource, CountBytes,
                   views[8]) ||
      !create_view(device, offset_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[9]) ||
      !create_view(device, total_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, TotalBytes,
                   views[10]) ||
      !create_view(device, offset_buffer,
                   gfx::IResourceView::Type::ShaderResource, CountBytes,
                   views[11]) ||
      !create_view(device, quad_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, QuadBytes,
                   views[12]) ||
      !create_view(device, emit_count_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[13]) ||
      !create_view(device, emit_sum_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[14]) ||
      !create_view(device, checksum_buffer,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[15])) {
    probe.status = "buffer_view_create_failed";
    return probe;
  }

  if (!succeeded(heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return probe;
  }
  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!succeeded(heap->createCommandBuffer(command_buffer.writeRef())) ||
      command_buffer == nullptr) {
    probe.status = "command_buffer_create_failed";
    return probe;
  }
  probe.command_buffer_created = true;

  gfx::IComputeCommandEncoder *encoder = nullptr;
  command_buffer->encodeComputeCommands(&encoder);
  if (encoder == nullptr) {
    probe.status = "compute_encoder_create_failed";
    return probe;
  }
  const std::array<gfx::IResourceView *, 5> face_views = {
      views[0], views[1], views[2], views[3], views[4]};
  const std::array<gfx::IResourceView *, 6> greedy_views = {
      views[0], views[1], views[2], views[5], views[6], views[7]};
  const std::array<gfx::IResourceView *, 3> prefix_views = {views[8], views[9],
                                                           views[10]};
  const std::array<gfx::IResourceView *, 10> emit_views = {
      views[0], views[1],  views[2],  views[5],  views[8],
      views[11], views[12], views[13], views[14], views[15]};
  if (!bind_resources(encoder, face_pipeline, face_views) ||
      !succeeded(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "face_mask_dispatch_failed";
    return probe;
  }
  probe.face_masks_dispatched = true;
  encoder->bufferBarrier(mask_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_resources(encoder, greedy_pipeline, greedy_views) ||
      !succeeded(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "greedy_count_dispatch_failed";
    return probe;
  }
  probe.greedy_dispatched = true;
  encoder->bufferBarrier(quad_count_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_resources(encoder, prefix_pipeline, prefix_views) ||
      !succeeded(encoder->dispatchCompute(1, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "prefix_range_dispatch_failed";
    return probe;
  }
  probe.prefix_dispatched = true;
  encoder->bufferBarrier(offset_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_resources(encoder, emit_pipeline, emit_views) ||
      !succeeded(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "packed_quad_emit_dispatch_failed";
    return probe;
  }
  probe.resources_bound = true;
  probe.emit_dispatched = true;
  encoder->bufferBarrier(total_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(quad_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(emit_count_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(emit_sum_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(checksum_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->endEncoding();
  encoder->release();
  command_buffer->close();

  gfx::IFence::Desc fence_desc{};
  Slang::ComPtr<gfx::IFence> fence;
  if (!succeeded(device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return probe;
  }
  queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;
  gfx::IFence *fences[] = {fence};
  std::uint64_t values[] = {1};
  if (!succeeded(
          device->waitForFences(1, fences, values, true, gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return probe;
  }
  probe.fence_completed = true;

  std::vector<std::uint32_t> actual_total(1u);
  std::vector<std::uint32_t> actual_counts(FaceMaskProbeChunkCount);
  std::vector<std::uint32_t> actual_sums(FaceMaskProbeChunkCount);
  std::vector<std::uint32_t> checksums(FaceMaskProbeChunkCount);
  std::vector<PackedVoxelQuad16> actual_quads(ExpectedTotalQuads);
  if (!readback_values(device, total_buffer, actual_total) ||
      !readback_values(device, emit_count_buffer, actual_counts) ||
      !readback_values(device, emit_sum_buffer, actual_sums) ||
      !readback_values(device, checksum_buffer, checksums) ||
      !readback_values(device, quad_buffer, actual_quads)) {
    probe.status = "packed_quad_emit_readback_failed";
    return probe;
  }

  probe.empty_count = actual_counts[0];
  probe.uniform_count = actual_counts[1];
  probe.checkerboard_count = actual_counts[2];
  probe.mixed_count = actual_counts[3];
  probe.total_quads = actual_total[0];
  probe.uniform_material_sum = actual_sums[1];
  probe.checkerboard_material_sum = actual_sums[2];
  probe.mixed_material_sum = actual_sums[3];
  for (auto checksum : checksums) {
    probe.nonzero_checksums += checksum != 0u ? 1u : 0u;
  }

  probe.readback_valid = actual_total[0] == ExpectedTotalQuads;
  for (std::uint32_t i = 0; i < FaceMaskProbeChunkCount; ++i) {
    probe.readback_valid &= actual_counts[i] == ExpectedCounts[i];
    probe.readback_valid &= actual_sums[i] == ExpectedMaterialSums[i];
    probe.readback_valid &= slang_rhi_voxel_packed_quad_ranges_match(
        actual_quads, ExpectedOffsets[i], ExpectedCounts[i],
        ExpectedMaterialSums[i]);
  }
  probe.readback_valid &= probe.nonzero_checksums == 3u;
  if (!probe.readback_valid) {
    probe.status = "packed_quad_emit_readback_mismatch";
    return probe;
  }
  if (!succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }
  probe.status = "gpu_voxel_packed_quads_validated";
  return probe;
#else
  return {false, false, false, false, false, false, false, false, false,
          false, false, false, false, false, false, 0u,    0u,    0u,
          0u,    0u,    0u,    0u,    0u,    0u,    "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
