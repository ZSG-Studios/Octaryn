#include "SlangComputePipeline.h"

#include "SlangRhiVoxelGreedyCounts.h"

#include "GpuChunkPayload.h"
#include "SlangRhiVoxelFaceMaskExpected.h"
#include "VoxelLayout.h"

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
using octaryn::client::voxel::GpuChunkPayload;

constexpr const char *FaceMaskShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelFaceMaskProbe.slang";
constexpr const char *GreedyCountShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelGreedyCountProbe.slang";
constexpr std::uint32_t EmptyChunk = 0u;
constexpr std::uint32_t UniformChunk = 1u;
constexpr std::uint32_t CheckerboardChunk = 2u;
constexpr std::uint32_t MixedChunk = 3u;
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount> ExpectedQuads = {
    0u, 6u, 98304u, 130u};
constexpr std::array<std::uint32_t, FaceMaskProbeChunkCount>
    ExpectedMaterialSums = {0u, 252u, 36372480u, 27950u};

struct GreedyBatch {
  std::vector<GpuChunkHeader> headers;
  std::vector<GpuChunkPaletteEntry> palette_entries;
  std::vector<std::uint8_t> payload_bytes;
};

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

std::uint32_t voxel_index(std::uint32_t x, std::uint32_t y,
                          std::uint32_t z) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  return x + W * (y + W * z);
}

std::vector<std::uint8_t> build_payload_bytes(std::uint32_t chunk_kind) {
  constexpr auto W = octaryn::client::voxel::ChunkWidthBlocks;
  const std::uint32_t payload_size =
      octaryn::client::voxel::packed_voxel_payload_bytes(2u);
  std::vector<std::uint8_t> bytes(payload_size, 0u);
  for (std::uint32_t z = 0; z < W; ++z) {
    for (std::uint32_t y = 0; y < W; ++y) {
      for (std::uint32_t x = 0; x < W; ++x) {
        const std::uint32_t palette_index =
            chunk_kind == CheckerboardChunk ? ((x + y + z) & 1u) : (x & 3u);
        const std::uint32_t bit_offset = voxel_index(x, y, z) * 2u;
        bytes[bit_offset >> 3u] |=
            static_cast<std::uint8_t>(palette_index << (bit_offset & 7u));
      }
    }
  }
  return bytes;
}

GpuChunkPayload make_checkerboard_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{37u, 370u, 1u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
  };
  const auto payload = build_payload_bytes(CheckerboardChunk);
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      CheckerboardChunk, 0u, entries.data(),
      static_cast<std::uint32_t>(entries.size()), payload.data(),
      static_cast<std::uint32_t>(payload.size()));
}

GpuChunkPayload make_mixed_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{20u, 200u, 1u, 0u},
      GpuChunkPaletteEntry{21u, 210u, 1u, 0u},
      GpuChunkPaletteEntry{22u, 220u, 1u, 0u},
      GpuChunkPaletteEntry{23u, 230u, 1u, 0u},
  };
  const auto payload = build_payload_bytes(MixedChunk);
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      MixedChunk, 0u, entries.data(),
      static_cast<std::uint32_t>(entries.size()), payload.data(),
      static_cast<std::uint32_t>(payload.size()));
}

bool append_chunk(GreedyBatch &batch, GpuChunkPayload chunk) {
  if (!octaryn::client::voxel::gpu_chunk_payload_valid(chunk)) { return false; }
  chunk.header.palette_offset =
      static_cast<std::uint32_t>(batch.palette_entries.size());
  chunk.header.voxel_data_offset =
      static_cast<std::uint32_t>(batch.payload_bytes.size());
  batch.headers.push_back(chunk.header);
  batch.palette_entries.insert(batch.palette_entries.end(),
                               chunk.palette.begin(), chunk.palette.end());
  batch.payload_bytes.insert(batch.payload_bytes.end(), chunk.payload.begin(),
                             chunk.payload.end());
  return true;
}

GreedyBatch build_greedy_batch() {
  GreedyBatch batch;
  append_chunk(batch, octaryn::client::voxel::make_empty_gpu_chunk(0u, 0u));
  append_chunk(batch,
               octaryn::client::voxel::make_uniform_gpu_chunk(1u, 0u, 42u));
  append_chunk(batch, make_checkerboard_payload());
  append_chunk(batch, make_mixed_payload());
  return batch;
}

std::vector<std::uint8_t>
make_payload_word_bytes(const std::vector<std::uint8_t> &payload) {
  std::vector<std::uint8_t> bytes = payload;
  while ((bytes.size() % sizeof(std::uint32_t)) != 0u) {
    bytes.push_back(0u);
  }
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

SlangRhiVoxelGreedyCountProbeResult make_result(const char *status) {
  return {true, false, false, false, false, false, false, false,
          false, false, false, false, false, false, 0u,    0u,
          0u,   0u,    0u,    0u,    0u,    0u,    status};
}

} // namespace
#endif

SlangRhiVoxelGreedyCountProbeResult probe_slang_rhi_voxel_greedy_counts() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  auto probe = make_result("not_started");
  const GreedyBatch batch = build_greedy_batch();
  const auto expected_masks = build_slang_rhi_voxel_face_mask_expected();
  if (batch.headers.size() != FaceMaskProbeChunkCount ||
      batch.palette_entries.size() != 9u || batch.payload_bytes.empty()) {
    probe.status = "greedy_count_batch_invalid";
    return probe;
  }

  const auto header_bytes = serialize_values(batch.headers);
  const auto palette_bytes = serialize_values(batch.palette_entries);
  const auto payload_bytes = make_payload_word_bytes(batch.payload_bytes);
  const std::size_t mask_bytes =
      expected_masks.masks.size() * sizeof(std::uint32_t);
  constexpr std::size_t CountBytes =
      FaceMaskProbeChunkCount * sizeof(std::uint32_t);

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
  heap_desc.srvDescriptorCount = 12;
  heap_desc.uavDescriptorCount = 12;
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
  if (!create_slang_compute_pipeline(device, FaceMaskShaderPath, face_pipeline) ||
      !create_slang_compute_pipeline(device, GreedyCountShaderPath, greedy_pipeline)) {
    probe.status = "compute_pipeline_create_failed";
    return probe;
  }
  probe.programs_created = true;
  probe.pipelines_created = true;

  const auto input_states =
      gfx::ResourceStateSet(gfx::ResourceState::ShaderResource);
  const auto mask_states = gfx::ResourceStateSet(
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
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     quad_count_buffer) ||
      !create_buffer(device, CountBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     material_sum_buffer)) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffers_created = true;

  std::array<Slang::ComPtr<gfx::IResourceView>, 9> owned_views;
  if (!create_buffer_view(device, header_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          header_bytes.size(), owned_views[0]) ||
      !create_buffer_view(device, palette_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          palette_bytes.size(), owned_views[1]) ||
      !create_buffer_view(device, payload_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          payload_bytes.size(), owned_views[2]) ||
      !create_buffer_view(device, mask_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, mask_bytes,
                          owned_views[3]) ||
      !create_buffer_view(device, face_count_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          owned_views[4]) ||
      !create_buffer_view(device, mask_buffer,
                          gfx::IResourceView::Type::ShaderResource, mask_bytes,
                          owned_views[5]) ||
      !create_buffer_view(device, quad_count_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          owned_views[6]) ||
      !create_buffer_view(device, material_sum_buffer,
                          gfx::IResourceView::Type::UnorderedAccess, CountBytes,
                          owned_views[7])) {
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
      owned_views[0], owned_views[1], owned_views[2], owned_views[3],
      owned_views[4]};
  const std::array<gfx::IResourceView *, 6> greedy_views = {
      owned_views[0], owned_views[1], owned_views[2], owned_views[5],
      owned_views[6], owned_views[7]};
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
  probe.resources_bound = true;
  if (!result_succeeded(
          encoder->dispatchCompute(FaceMaskProbeChunkCount, 1, 1))) {
    encoder->endEncoding();
    encoder->release();
    probe.status = "greedy_count_dispatch_failed";
    return probe;
  }
  probe.greedy_dispatched = true;
  encoder->bufferBarrier(quad_count_buffer, gfx::ResourceState::UnorderedAccess,
                         gfx::ResourceState::CopySource);
  encoder->bufferBarrier(material_sum_buffer,
                         gfx::ResourceState::UnorderedAccess,
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

  std::vector<std::uint32_t> actual_quads(FaceMaskProbeChunkCount);
  std::vector<std::uint32_t> actual_material_sums(FaceMaskProbeChunkCount);
  if (!readback_u32(device, quad_count_buffer, actual_quads) ||
      !readback_u32(device, material_sum_buffer, actual_material_sums)) {
    probe.status = "greedy_count_readback_failed";
    return probe;
  }
  probe.empty_quads = actual_quads[EmptyChunk];
  probe.uniform_quads = actual_quads[UniformChunk];
  probe.checkerboard_quads = actual_quads[CheckerboardChunk];
  probe.mixed_quads = actual_quads[MixedChunk];
  probe.empty_material_sum = actual_material_sums[EmptyChunk];
  probe.uniform_material_sum = actual_material_sums[UniformChunk];
  probe.checkerboard_material_sum = actual_material_sums[CheckerboardChunk];
  probe.mixed_material_sum = actual_material_sums[MixedChunk];
  probe.readback_valid = true;
  for (std::uint32_t i = 0; i < FaceMaskProbeChunkCount; ++i) {
    probe.readback_valid &= actual_quads[i] == ExpectedQuads[i];
    probe.readback_valid &= actual_material_sums[i] == ExpectedMaterialSums[i];
  }
  if (!probe.readback_valid) {
    probe.status = "greedy_count_readback_mismatch";
    return probe;
  }
  if (!result_succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }
  probe.status = "gpu_voxel_greedy_counts_validated";
  return probe;
#else
  return {false, false, false, false, false, false, false, false,
          false, false, false, false, false, false, 0u,    0u,
          0u,   0u,    0u,    0u,    0u,    0u,    "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
