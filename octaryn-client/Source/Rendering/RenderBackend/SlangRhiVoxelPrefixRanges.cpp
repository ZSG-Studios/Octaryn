#include "SlangComputePipeline.h"

#include "SlangRhiVoxelPrefixRanges.h"

#include "SlangRhiVoxelFaceMaskExpected.h"
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

constexpr const char *FaceMaskShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelFaceMaskProbe.slang";
constexpr const char *GreedyCountShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelGreedyCountProbe.slang";
constexpr const char *PrefixScanShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelPrefixScanProbe.slang";
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount> ExpectedOffsets = {
    0u, 0u, 6u, 98310u};
constexpr std::uint32_t ExpectedTotalQuads = 98440u;

bool result_succeeded(SlangResult result) { return SLANG_SUCCEEDED(result); }

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
  return result_succeeded(device->createBufferResource(
             desc, initial_data, buffer.writeRef())) &&
         buffer != nullptr;
}

bool create_buffer_view(gfx::IDevice *device, gfx::IBufferResource *buffer,
                        gfx::IResourceView::Type type, std::size_t byte_count,
                        Slang::ComPtr<gfx::IResourceView> &view) {
  gfx::IResourceView::Desc desc{};
  desc.type = type;
  desc.format = gfx::Format::Unknown;
  desc.bufferRange.offset = 0;
  desc.bufferRange.size = byte_count;
  return result_succeeded(
             device->createBufferView(buffer, nullptr, desc, view.writeRef())) &&
         view != nullptr;
}



bool set_resource(gfx::IShaderObject *object, gfx::GfxIndex index,
                  gfx::IResourceView *view) {
  return object != nullptr &&
         result_succeeded(object->setResource({0, index, 0}, view));
}

template <std::size_t N>
bool bind_resources(gfx::IComputeCommandEncoder *encoder,
                    gfx::IPipelineState *pipeline,
                    const std::array<gfx::IResourceView *, N> &views) {
  Slang::ComPtr<gfx::IShaderObject> root;
  if (!result_succeeded(encoder->bindPipeline(pipeline, root.writeRef())) ||
      root == nullptr) {
    return false;
  }
  Slang::ComPtr<gfx::IShaderObject> entry_point;
  root->getEntryPoint(0, entry_point.writeRef());
  bool root_ok = true;
  bool entry_ok = true;
  for (std::size_t index = 0; index < views.size(); ++index) {
    const auto binding_index = static_cast<gfx::GfxIndex>(index);
    root_ok &= set_resource(root, binding_index, views[index]);
    entry_ok &= set_resource(entry_point, binding_index, views[index]);
  }
  return root_ok || entry_ok;
}

bool readback_u32(gfx::IDevice *device, gfx::IBufferResource *buffer,
                  std::vector<std::uint32_t> &values) {
  Slang::ComPtr<ISlangBlob> readback;
  const std::size_t bytes = values.size() * sizeof(std::uint32_t);
  if (!result_succeeded(device->readBufferResource(
          buffer, 0, bytes, readback.writeRef())) ||
      readback == nullptr || readback->getBufferSize() != bytes) {
    return false;
  }
  std::memcpy(values.data(), readback->getBufferPointer(), bytes);
  return true;
}

SlangRhiVoxelPrefixRangeProbeResult make_result(const char *status) {
  return {true, false, false, false, false, false, false, false,
          false, false, false, false, false, false, 0u,    0u,
          0u,   0u,    0u,    status};
}

} // namespace
#endif

SlangRhiVoxelPrefixRangeProbeResult probe_slang_rhi_voxel_prefix_ranges() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  auto probe = make_result("not_started");
  const auto batch = build_slang_rhi_voxel_prefix_probe_batch();
  const auto expected_masks = build_slang_rhi_voxel_face_mask_expected();
  if (batch.headers.size() != FaceMaskProbeChunkCount ||
      batch.palette_entries.size() != 9u || batch.payload_bytes.empty()) {
    probe.status = "prefix_range_batch_invalid";
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

  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  Slang::ComPtr<gfx::IDevice> device;
  if (!result_succeeded(gfx::gfxCreateDevice(&device_desc, device.writeRef())) ||
      device == nullptr) {
    probe.status = "device_create_failed";
    return probe;
  }
  probe.device_created = true;

  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  if (!result_succeeded(device->createCommandQueue(queue_desc, queue.writeRef())) ||
      queue == nullptr) {
    probe.status = "queue_create_failed";
    return probe;
  }
  probe.queue_created = true;

  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 16;
  heap_desc.uavDescriptorCount = 16;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!result_succeeded(
          device->createTransientResourceHeap(heap_desc, heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  Slang::ComPtr<gfx::IPipelineState> face_pipeline;
  Slang::ComPtr<gfx::IPipelineState> greedy_pipeline;
  Slang::ComPtr<gfx::IPipelineState> prefix_pipeline;
  if (!create_slang_compute_pipeline(device, FaceMaskShaderPath, face_pipeline) ||
      !create_slang_compute_pipeline(device, GreedyCountShaderPath, greedy_pipeline) ||
      !create_slang_compute_pipeline(device, PrefixScanShaderPath, prefix_pipeline)) {
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
  Slang::ComPtr<gfx::IBufferResource> header_buffer;
  Slang::ComPtr<gfx::IBufferResource> palette_buffer;
  Slang::ComPtr<gfx::IBufferResource> payload_buffer;
  Slang::ComPtr<gfx::IBufferResource> mask_buffer;
  Slang::ComPtr<gfx::IBufferResource> face_count_buffer;
  Slang::ComPtr<gfx::IBufferResource> quad_count_buffer;
  Slang::ComPtr<gfx::IBufferResource> material_sum_buffer;
  Slang::ComPtr<gfx::IBufferResource> offset_buffer;
  Slang::ComPtr<gfx::IBufferResource> total_buffer;
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
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     offset_buffer) ||
      !create_buffer(device, TotalBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     total_buffer)) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffers_created = true;

  std::array<Slang::ComPtr<gfx::IResourceView>, 11> views;
  if (!create_buffer_view(device, header_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          header_bytes.size(), views[0]) ||
      !create_buffer_view(device, palette_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          palette_bytes.size(), views[1]) ||
      !create_buffer_view(device, payload_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          payload_bytes.size(), views[2]) ||
      !create_buffer_view(device, mask_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, mask_bytes,
                          views[3]) ||
      !create_buffer_view(device, face_count_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          views[4]) ||
      !create_buffer_view(device, mask_buffer,
                          gfx::IResourceView::Type::ShaderResource, mask_bytes,
                          views[5]) ||
      !create_buffer_view(device, quad_count_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          views[6]) ||
      !create_buffer_view(device, material_sum_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          views[7]) ||
      !create_buffer_view(device, quad_count_buffer,
                          gfx::IResourceView::Type::ShaderResource, CountBytes,
                          views[8]) ||
      !create_buffer_view(device, offset_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          views[9]) ||
      !create_buffer_view(device, total_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, TotalBytes,
                          views[10])) {
    probe.status = "buffer_view_create_failed";
    return probe;
  }

  if (!result_succeeded(heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return probe;
  }
  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!result_succeeded(heap->createCommandBuffer(command_buffer.writeRef())) ||
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
  if (!bind_resources(encoder, face_pipeline, face_views)) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "face_mask_resources_bind_failed";
    return probe;
  }
  if (!result_succeeded(
          encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "face_mask_dispatch_failed";
    return probe;
  }
  probe.face_masks_dispatched = true;
  encoder->bufferBarrier(mask_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_resources(encoder, greedy_pipeline, greedy_views)) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "greedy_count_resources_bind_failed";
    return probe;
  }
  if (!result_succeeded(
          encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "greedy_count_dispatch_failed";
    return probe;
  }
  probe.greedy_dispatched = true;
  encoder->bufferBarrier(quad_count_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind_resources(encoder, prefix_pipeline, prefix_views)) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "prefix_range_resources_bind_failed";
    return probe;
  }
  probe.resources_bound = true;
  if (!result_succeeded(encoder->dispatchCompute(1, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "prefix_range_dispatch_failed";
    return probe;
  }
  probe.prefix_dispatched = true;
  encoder->bufferBarrier(offset_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(total_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->endEncoding();
  encoder->release();
  command_buffer->close();

  gfx::IFence::Desc fence_desc{};
  Slang::ComPtr<gfx::IFence> fence;
  if (!result_succeeded(device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return probe;
  }
  queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;
  gfx::IFence *fences[] = {fence};
  std::uint64_t values[] = {1};
  if (!result_succeeded(
          device->waitForFences(1, fences, values, true, gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return probe;
  }
  probe.fence_completed = true;

  std::vector<std::uint32_t> actual_offsets(FaceMaskProbeChunkCount);
  std::vector<std::uint32_t> actual_total(1u);
  if (!readback_u32(device, offset_buffer, actual_offsets) ||
      !readback_u32(device, total_buffer, actual_total)) {
    probe.status = "prefix_range_readback_failed";
    return probe;
  }
  probe.empty_offset = actual_offsets[0];
  probe.uniform_offset = actual_offsets[1];
  probe.checkerboard_offset = actual_offsets[2];
  probe.mixed_offset = actual_offsets[3];
  probe.total_quads = actual_total[0];
  probe.readback_valid = actual_total[0] == ExpectedTotalQuads;
  for (std::uint32_t i = 0; i < FaceMaskProbeChunkCount; ++i) {
    probe.readback_valid &= actual_offsets[i] == ExpectedOffsets[i];
  }
  if (!probe.readback_valid) {
    probe.status = "prefix_range_readback_mismatch";
    return probe;
  }
  if (!result_succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }
  probe.status = "gpu_voxel_prefix_ranges_validated";
  return probe;
#else
  return {false, false, false, false, false, false, false, false,
          false, false, false, false, false, false, 0u,    0u,
          0u,   0u,    0u,    "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
