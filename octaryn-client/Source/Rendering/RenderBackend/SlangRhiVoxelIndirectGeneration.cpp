#include "SlangComputePipeline.h"

#include "SlangRhiVoxelIndirectGeneration.h"
#include "SlangRhiVoxelFaceMaskExpected.h"
#include "SlangRhiVoxelIndirectValidation.h"
#include "SlangRhiVoxelPrefixProbeBatch.h"
#include "VoxelIndirect.h"
#include "PackedVoxelQuad.h"
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
using octaryn::client::voxel::DrawIndexedIndirectCommand;
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
constexpr const char *IndirectShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelIndirectGenerationProbe.slang";
constexpr std::uint32_t ExpectedTotalQuads = 98440u;
bool ok(SlangResult result) { return SLANG_SUCCEEDED(result); }
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
  return ok(device->createBufferResource(desc, initial_data,
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
  return ok(device->createBufferView(buffer, nullptr, desc, view.writeRef())) &&
         view != nullptr;
}



bool set_resource(gfx::IShaderObject *object, gfx::GfxIndex index,
                  gfx::IResourceView *view) {
  return object != nullptr && ok(object->setResource({0, index, 0}, view));
}
template <std::size_t N>
bool bind(gfx::IComputeCommandEncoder *encoder, gfx::IPipelineState *pipeline,
          const std::array<gfx::IResourceView *, N> &views) {
  Slang::ComPtr<gfx::IShaderObject> root;
  if (!ok(encoder->bindPipeline(pipeline, root.writeRef())) || root == nullptr) {
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
bool readback(gfx::IDevice *device, gfx::IBufferResource *buffer,
              std::vector<T> &values) {
  Slang::ComPtr<ISlangBlob> readback_blob;
  const std::size_t bytes = values.size() * sizeof(T);
  if (!ok(device->readBufferResource(buffer, 0, bytes,
                                     readback_blob.writeRef())) ||
      readback_blob == nullptr || readback_blob->getBufferSize() != bytes) {
    return false;
  }
  std::memcpy(values.data(), readback_blob->getBufferPointer(), bytes);
  return true;
}
SlangRhiVoxelIndirectGenerationProbeResult result(const char *status) {
  return {true, false, false, false, false, false, false, false,
          false, false, false, false, false, false, false, false,
          0u,   0u,    0u,    0u,    0u,    0u,    0u,    status};
}

} // namespace
#endif
SlangRhiVoxelIndirectGenerationProbeResult
probe_slang_rhi_voxel_indirect_generation() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  auto probe = result("not_started");
  const auto batch = build_slang_rhi_voxel_prefix_probe_batch();
  const auto masks = build_slang_rhi_voxel_face_mask_expected();
  if (batch.headers.size() != FaceMaskProbeChunkCount ||
      batch.palette_entries.size() != 9u || batch.payload_bytes.empty()) {
    probe.status = "indirect_generation_batch_invalid";
    return probe;
  }

  const auto header_bytes = serialize_values(batch.headers);
  const auto palette_bytes = serialize_values(batch.palette_entries);
  const auto payload_bytes =
      make_slang_rhi_voxel_prefix_payload_word_bytes(batch.payload_bytes);
  const std::size_t mask_bytes = masks.masks.size() * sizeof(std::uint32_t);
  constexpr std::size_t CountBytes =
      FaceMaskProbeChunkCount * sizeof(std::uint32_t);
  constexpr std::size_t ScalarBytes = sizeof(std::uint32_t);
  constexpr std::size_t QuadBytes =
      ExpectedTotalQuads * sizeof(PackedVoxelQuad16);
  constexpr std::size_t CommandBytes =
      FaceMaskProbeChunkCount * sizeof(DrawIndexedIndirectCommand);

  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  Slang::ComPtr<gfx::IDevice> device;
  if (!ok(gfx::gfxCreateDevice(&device_desc, device.writeRef())) ||
      device == nullptr) {
    probe.status = "device_create_failed";
    return probe;
  }
  probe.device_created = true;
  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  if (!ok(device->createCommandQueue(queue_desc, queue.writeRef())) ||
      queue == nullptr) {
    probe.status = "queue_create_failed";
    return probe;
  }
  probe.queue_created = true;
  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 24;
  heap_desc.uavDescriptorCount = 24;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!ok(device->createTransientResourceHeap(heap_desc, heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  Slang::ComPtr<gfx::IPipelineState> face_pipe, greedy_pipe, prefix_pipe;
  Slang::ComPtr<gfx::IPipelineState> emit_pipe, indirect_pipe;
  if (!create_slang_compute_pipeline(device, FaceMaskShaderPath, face_pipe) ||
      !create_slang_compute_pipeline(device, GreedyCountShaderPath, greedy_pipe) ||
      !create_slang_compute_pipeline(device, PrefixScanShaderPath, prefix_pipe) ||
      !create_slang_compute_pipeline(device, PackedEmitShaderPath, emit_pipe) ||
      !create_slang_compute_pipeline(device, IndirectShaderPath, indirect_pipe)) {
    probe.status = "compute_pipeline_create_failed";
    return probe;
  }
  probe.pipelines_created = true;

  const auto srv = gfx::ResourceStateSet(gfx::ResourceState::ShaderResource);
  const auto uav_copy = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::CopySource);
  const auto uav_srv = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource);
  const auto uav_srv_copy = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::ShaderResource,
      gfx::ResourceState::CopySource);

  Slang::ComPtr<gfx::IBufferResource> headers, palette, payload, face_masks;
  Slang::ComPtr<gfx::IBufferResource> face_counts, quad_counts, material_sums;
  Slang::ComPtr<gfx::IBufferResource> offsets, total, quads, emit_counts;
  Slang::ComPtr<gfx::IBufferResource> emit_sums, emit_checksums, commands;
  Slang::ComPtr<gfx::IBufferResource> draw_count, empty_count, instances;
  Slang::ComPtr<gfx::IBufferResource> first_quad_checksums;
  if (!create_buffer(device, header_bytes.size(), sizeof(GpuChunkHeader),
                     gfx::ResourceState::ShaderResource, srv,
                     header_bytes.data(), headers) ||
      !create_buffer(device, palette_bytes.size(), sizeof(GpuChunkPaletteEntry),
                     gfx::ResourceState::ShaderResource, srv,
                     palette_bytes.data(), palette) ||
      !create_buffer(device, payload_bytes.size(), sizeof(std::uint32_t),
                     gfx::ResourceState::ShaderResource, srv,
                     payload_bytes.data(), payload) ||
      !create_buffer(device, mask_bytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_srv, nullptr,
                     face_masks) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     face_counts) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_srv_copy, nullptr,
                     quad_counts) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     material_sums) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_srv_copy, nullptr,
                     offsets) ||
      !create_buffer(device, ScalarBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     total) ||
      !create_buffer(device, QuadBytes, sizeof(PackedVoxelQuad16),
                     gfx::ResourceState::UnorderedAccess, uav_srv_copy, nullptr,
                     quads) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_srv_copy, nullptr,
                     emit_counts) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     emit_sums) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     emit_checksums) ||
      !create_buffer(device, CommandBytes, sizeof(DrawIndexedIndirectCommand),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     commands) ||
      !create_buffer(device, ScalarBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     draw_count) ||
      !create_buffer(device, ScalarBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     empty_count) ||
      !create_buffer(device, ScalarBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     instances) ||
      !create_buffer(device, ScalarBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, uav_copy, nullptr,
                     first_quad_checksums)) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffers_created = true;

  std::array<Slang::ComPtr<gfx::IResourceView>, 23> views;
  if (!create_view(device, headers, gfx::IResourceView::Type::ShaderResource,
                   header_bytes.size(), views[0]) ||
      !create_view(device, palette, gfx::IResourceView::Type::ShaderResource,
                   palette_bytes.size(), views[1]) ||
      !create_view(device, payload, gfx::IResourceView::Type::ShaderResource,
                   payload_bytes.size(), views[2]) ||
      !create_view(device, face_masks, gfx::IResourceView::Type::UnorderedAccess,
                   mask_bytes, views[3]) ||
      !create_view(device, face_counts,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[4]) ||
      !create_view(device, face_masks, gfx::IResourceView::Type::ShaderResource,
                   mask_bytes, views[5]) ||
      !create_view(device, quad_counts,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[6]) ||
      !create_view(device, material_sums,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[7]) ||
      !create_view(device, quad_counts,
                   gfx::IResourceView::Type::ShaderResource, CountBytes,
                   views[8]) ||
      !create_view(device, offsets, gfx::IResourceView::Type::UnorderedAccess,
                   CountBytes, views[9]) ||
      !create_view(device, total, gfx::IResourceView::Type::UnorderedAccess,
                   ScalarBytes, views[10]) ||
      !create_view(device, offsets, gfx::IResourceView::Type::ShaderResource,
                   CountBytes, views[11]) ||
      !create_view(device, quads, gfx::IResourceView::Type::UnorderedAccess,
                   QuadBytes, views[12]) ||
      !create_view(device, emit_counts,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[13]) ||
      !create_view(device, emit_sums, gfx::IResourceView::Type::UnorderedAccess,
                   CountBytes, views[14]) ||
      !create_view(device, emit_checksums,
                   gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                   views[15]) ||
      !create_view(device, quads, gfx::IResourceView::Type::ShaderResource,
                   QuadBytes, views[16]) ||
      !create_view(device, emit_counts,
                   gfx::IResourceView::Type::ShaderResource, CountBytes,
                   views[17]) ||
      !create_view(device, commands, gfx::IResourceView::Type::UnorderedAccess,
                   CommandBytes, views[18]) ||
      !create_view(device, draw_count,
                   gfx::IResourceView::Type::UnorderedAccess, ScalarBytes,
                   views[19]) ||
      !create_view(device, empty_count,
                   gfx::IResourceView::Type::UnorderedAccess, ScalarBytes,
                   views[20]) ||
      !create_view(device, instances, gfx::IResourceView::Type::UnorderedAccess,
                   ScalarBytes, views[21]) ||
      !create_view(device, first_quad_checksums,
                   gfx::IResourceView::Type::UnorderedAccess, ScalarBytes,
                   views[22])) {
    probe.status = "buffer_view_create_failed";
    return probe;
  }

  if (!ok(heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return probe;
  }
  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!ok(heap->createCommandBuffer(command_buffer.writeRef())) ||
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
  const std::array<gfx::IResourceView *, 8> indirect_views = {
      views[16], views[17], views[11], views[18],
      views[19], views[20], views[21], views[22]};
  if (!bind(encoder, face_pipe, face_views) ||
      !ok(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "face_mask_dispatch_failed";
    return probe;
  }
  probe.face_masks_dispatched = true;
  encoder->bufferBarrier(face_masks, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind(encoder, greedy_pipe, greedy_views) ||
      !ok(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "greedy_count_dispatch_failed";
    return probe;
  }
  probe.greedy_dispatched = true;
  encoder->bufferBarrier(quad_counts, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind(encoder, prefix_pipe, prefix_views) ||
      !ok(encoder->dispatchCompute(1, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "prefix_range_dispatch_failed";
    return probe;
  }
  probe.prefix_dispatched = true;
  encoder->bufferBarrier(offsets, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind(encoder, emit_pipe, emit_views) ||
      !ok(encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "packed_quad_emit_dispatch_failed";
    return probe;
  }
  probe.emit_dispatched = true;
  encoder->bufferBarrier(quads, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  encoder->bufferBarrier(emit_counts, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::General);
  if (!bind(encoder, indirect_pipe, indirect_views) ||
      !ok(encoder->dispatchCompute(1, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "indirect_generation_dispatch_failed";
    return probe;
  }
  probe.resources_bound = true;
  probe.indirect_dispatched = true;
  encoder->bufferBarrier(commands, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(draw_count, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(empty_count, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(instances, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(first_quad_checksums,
                         gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->endEncoding();
  encoder->release();
  command_buffer->close();

  gfx::IFence::Desc fence_desc{};
  Slang::ComPtr<gfx::IFence> fence;
  if (!ok(device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return probe;
  }
  queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;
  gfx::IFence *fences[] = {fence};
  std::uint64_t fence_values[] = {1};
  if (!ok(device->waitForFences(1, fences, fence_values, true,
                                gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return probe;
  }
  probe.fence_completed = true;

  std::vector<DrawIndexedIndirectCommand> actual_commands(FaceMaskProbeChunkCount);
  std::vector<std::uint32_t> actual_draw_count(1u), actual_empty_count(1u);
  std::vector<std::uint32_t> actual_instances(1u), actual_checksum(1u);
  if (!readback(device, commands, actual_commands) ||
      !readback(device, draw_count, actual_draw_count) ||
      !readback(device, empty_count, actual_empty_count) ||
      !readback(device, instances, actual_instances) ||
      !readback(device, first_quad_checksums, actual_checksum)) {
    probe.status = "indirect_generation_readback_failed";
    return probe;
  }

  probe.draw_count = actual_draw_count[0];
  probe.empty_draw_count = actual_empty_count[0];
  probe.total_instances = actual_instances[0];
  probe.first_instance_uniform = actual_commands[0].first_instance;
  probe.first_instance_checkerboard = actual_commands[1].first_instance;
  probe.first_instance_mixed = actual_commands[2].first_instance;
  probe.checksum_nonzero = actual_checksum[0] != 0u ? 1u : 0u;
  probe.readback_valid = slang_rhi_voxel_indirect_commands_match(
      actual_commands, probe.draw_count, probe.empty_draw_count,
      probe.total_instances, actual_checksum[0]);
  if (!probe.readback_valid) {
    probe.status = "indirect_generation_readback_mismatch";
    return probe;
  }
  if (!ok(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }
  probe.status = "gpu_voxel_indirect_commands_validated";
  return probe;
#else
  return {false, false, false, false, false, false, false, false,
          false, false, false, false, false, false, false, false,
          0u,    0u,    0u,    0u,    0u,    0u,    0u,
          "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
