#include "SlangComputePipeline.h"

#include "SlangRhiVoxelOccupancy.h"

#include "ChunkPalette.h"
#include "GpuChunkPayload.h"
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

using octaryn::client::voxel::ChunkPaletteEmpty;
using octaryn::client::voxel::ChunkPaletteMixed;
using octaryn::client::voxel::ChunkPaletteUniform;
using octaryn::client::voxel::ChunkVoxelCount;
using octaryn::client::voxel::GpuChunkHeader;
using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::GpuChunkPayload;

constexpr const char *OccupancyShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelOccupancyProbe.slang";
constexpr std::uint32_t EmptyExpectedOccupancy = 0u;
constexpr std::uint32_t UniformExpectedOccupancy =
    static_cast<std::uint32_t>(ChunkVoxelCount);
constexpr std::uint32_t MixedExpectedOccupancy =
    static_cast<std::uint32_t>((ChunkVoxelCount / 4) * 3);

bool result_succeeded(SlangResult result) { return SLANG_SUCCEEDED(result); }

void append_bytes(std::vector<std::uint8_t> &bytes, const void *source,
                  std::size_t byte_count) {
  if (byte_count == 0u) {
    return;
  }
  const auto *typed_source = static_cast<const std::uint8_t *>(source);
  bytes.insert(bytes.end(), typed_source, typed_source + byte_count);
}

template <typename T>
std::vector<std::uint8_t> serialize_values(const std::vector<T> &values) {
  std::vector<std::uint8_t> bytes;
  append_bytes(bytes, values.data(), values.size() * sizeof(T));
  return bytes;
}

struct OccupancyBatch {
  std::vector<GpuChunkHeader> headers;
  std::vector<GpuChunkPaletteEntry> palette_entries;
  std::vector<std::uint8_t> payload_bytes;
};

std::vector<std::uint8_t> build_mixed_payload_bytes() {
  const std::uint32_t payload_size =
      octaryn::client::voxel::packed_voxel_payload_bytes(2u);
  std::vector<std::uint8_t> bytes(payload_size, 0u);
  for (std::uint32_t voxel = 0; voxel < ChunkVoxelCount; ++voxel) {
    const std::uint32_t palette_index = voxel & 3u;
    const std::uint32_t bit_offset = voxel * 2u;
    bytes[bit_offset >> 3u] |=
        static_cast<std::uint8_t>(palette_index << (bit_offset & 7u));
  }
  return bytes;
}

GpuChunkPayload build_mixed_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{0u, 0u, 0u, 0u},
      GpuChunkPaletteEntry{21u, 210u, 1u, 0u},
      GpuChunkPaletteEntry{22u, 220u, 1u, 0u},
      GpuChunkPaletteEntry{23u, 230u, 1u, 0u},
  };
  const auto payload = build_mixed_payload_bytes();
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      2u, 0u, entries.data(), static_cast<std::uint32_t>(entries.size()),
      payload.data(), static_cast<std::uint32_t>(payload.size()));
}

bool append_chunk(OccupancyBatch &batch, GpuChunkPayload chunk) {
  if (!octaryn::client::voxel::gpu_chunk_payload_valid(chunk)) {
    return false;
  }
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

OccupancyBatch build_occupancy_batch() {
  OccupancyBatch batch;
  append_chunk(batch, octaryn::client::voxel::make_empty_gpu_chunk(0u, 0u));
  append_chunk(batch,
               octaryn::client::voxel::make_uniform_gpu_chunk(1u, 0u, 42u));
  append_chunk(batch, build_mixed_payload());
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
                   std::size_t element_size, gfx::ResourceState default_state,
                   gfx::ResourceStateSet allowed_states,
                   const void *initial_data,
                   Slang::ComPtr<gfx::IBufferResource> &buffer) {
  gfx::IBufferResource::Desc desc{};
  desc.type = gfx::IResource::Type::Buffer;
  desc.defaultState = default_state;
  desc.allowedStates = allowed_states;
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

bool readback_counts(gfx::IDevice *device, gfx::IBufferResource *buffer,
                     std::array<std::uint32_t, 3> &counts) {
  Slang::ComPtr<ISlangBlob> readback;
  const std::size_t byte_count = counts.size() * sizeof(std::uint32_t);
  if (!result_succeeded(device->readBufferResource(
          buffer, 0, byte_count, readback.writeRef())) ||
      readback == nullptr || readback->getBufferSize() != byte_count) {
    return false;
  }
  std::memcpy(counts.data(), readback->getBufferPointer(), byte_count);
  return true;
}



bool bind_shader_object_resources(gfx::IShaderObject *shader_object,
                                  gfx::IResourceView *header_view,
                                  gfx::IResourceView *palette_view,
                                  gfx::IResourceView *payload_view,
                                  gfx::IResourceView *output_view) {
  if (shader_object == nullptr) {
    return false;
  }
  const gfx::ShaderOffset header_offset{0, 0, 0};
  const gfx::ShaderOffset palette_offset{0, 1, 0};
  const gfx::ShaderOffset payload_offset{0, 2, 0};
  const gfx::ShaderOffset output_offset{0, 3, 0};
  return result_succeeded(shader_object->setResource(header_offset, header_view)) &&
         result_succeeded(shader_object->setResource(palette_offset, palette_view)) &&
         result_succeeded(shader_object->setResource(payload_offset, payload_view)) &&
         result_succeeded(shader_object->setResource(output_offset, output_view));
}

bool bind_compute_resources(
    gfx::IComputeCommandEncoder *encoder, gfx::IPipelineState *pipeline,
    gfx::IResourceView *header_view, gfx::IResourceView *palette_view,
    gfx::IResourceView *payload_view, gfx::IResourceView *output_view,
    Slang::ComPtr<gfx::IShaderObject> &root,
    Slang::ComPtr<gfx::IShaderObject> &entry_point) {
  if (!result_succeeded(encoder->bindPipeline(pipeline, root.writeRef())) ||
      root == nullptr) {
    return false;
  }
  root->getEntryPoint(0, entry_point.writeRef());
  const bool root_bound = bind_shader_object_resources(
      root, header_view, palette_view, payload_view, output_view);
  const bool entry_bound = bind_shader_object_resources(
      entry_point, header_view, palette_view, payload_view, output_view);
  return root_bound || entry_bound;
}

SlangRhiVoxelOccupancyProbeResult make_result(const char *status) {
  return {true, false, false, false, false, false, false, false, false, false,
          false, false, false, 0u,    0u,    0u,    status};
}

} // namespace
#endif

SlangRhiVoxelOccupancyProbeResult
probe_slang_rhi_voxel_occupancy_decode() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  SlangRhiVoxelOccupancyProbeResult probe = make_result("not_started");

  const OccupancyBatch batch = build_occupancy_batch();
  if (batch.headers.size() != 3u || batch.palette_entries.size() != 5u ||
      batch.payload_bytes.empty()) {
    probe.status = "occupancy_batch_invalid";
    return probe;
  }

  const std::vector<std::uint8_t> header_bytes =
      serialize_values(batch.headers);
  const std::vector<std::uint8_t> palette_bytes =
      serialize_values(batch.palette_entries);
  const std::vector<std::uint8_t> payload_bytes =
      make_payload_word_bytes(batch.payload_bytes);
  constexpr std::size_t OutputBytes = 3u * sizeof(std::uint32_t);

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
  heap_desc.flags = gfx::ITransientResourceHeap::Flags::None;
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 8;
  heap_desc.uavDescriptorCount = 4;
  heap_desc.constantBufferDescriptorCount = 4;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!result_succeeded(device->createTransientResourceHeap(heap_desc,
                                                           heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  Slang::ComPtr<gfx::IPipelineState> pipeline;
  if (!create_slang_compute_pipeline(device, OccupancyShaderPath, pipeline)) {
    probe.status = "compute_pipeline_create_failed";
    return probe;
  }
  probe.program_created = true;
  probe.pipeline_created = true;

  Slang::ComPtr<gfx::IBufferResource> header_buffer;
  Slang::ComPtr<gfx::IBufferResource> palette_buffer;
  Slang::ComPtr<gfx::IBufferResource> payload_buffer;
  Slang::ComPtr<gfx::IBufferResource> output_buffer;
  const auto input_states =
      gfx::ResourceStateSet(gfx::ResourceState::ShaderResource);
  const auto output_states = gfx::ResourceStateSet(
      gfx::ResourceState::UnorderedAccess, gfx::ResourceState::CopySource);
  if (!create_buffer(device, header_bytes.size(), sizeof(GpuChunkHeader),
                     gfx::ResourceState::ShaderResource, input_states,
                     header_bytes.data(),
                     header_buffer) ||
      !create_buffer(device, palette_bytes.size(), sizeof(GpuChunkPaletteEntry),
                     gfx::ResourceState::ShaderResource, input_states,
                     palette_bytes.data(),
                     palette_buffer) ||
      !create_buffer(device, payload_bytes.size(), sizeof(std::uint32_t),
                     gfx::ResourceState::ShaderResource, input_states,
                     payload_bytes.data(),
                     payload_buffer) ||
      !create_buffer(device, OutputBytes, sizeof(std::uint32_t),
                     gfx::ResourceState::UnorderedAccess, output_states, nullptr,
                     output_buffer)) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffers_created = true;

  Slang::ComPtr<gfx::IResourceView> header_view;
  Slang::ComPtr<gfx::IResourceView> palette_view;
  Slang::ComPtr<gfx::IResourceView> payload_view;
  Slang::ComPtr<gfx::IResourceView> output_view;
  if (!create_buffer_view(device, header_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          header_bytes.size(), header_view) ||
      !create_buffer_view(device, palette_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          palette_bytes.size(), palette_view) ||
      !create_buffer_view(device, payload_buffer,
                          gfx::IResourceView::Type::ShaderResource,
                          payload_bytes.size(), payload_view) ||
      !create_buffer_view(device, output_buffer,
                          gfx::IResourceView::Type::UnorderedAccess,
                          OutputBytes, output_view)) {
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

  gfx::IComputeCommandEncoder *compute_encoder = nullptr;
  command_buffer->encodeComputeCommands(&compute_encoder);
  if (compute_encoder == nullptr) {
    probe.status = "compute_encoder_create_failed";
    return probe;
  }
  Slang::ComPtr<gfx::IShaderObject> root_shader_object;
  Slang::ComPtr<gfx::IShaderObject> entry_point_shader_object;
  if (!bind_compute_resources(compute_encoder, pipeline, header_view,
                              palette_view, payload_view, output_view,
                              root_shader_object, entry_point_shader_object)) {
    compute_encoder->endEncoding();
    compute_encoder->release();
    probe.status = "compute_resources_bind_failed";
    return probe;
  }
  probe.resources_bound = true;

  if (!result_succeeded(compute_encoder->dispatchCompute(3, 1, 1))) {
    compute_encoder->endEncoding();
    compute_encoder->release();
    probe.status = "compute_dispatch_failed";
    return probe;
  }
  probe.dispatched = true;
  compute_encoder->bufferBarrier(output_buffer, gfx::ResourceState::UnorderedAccess,
                                 gfx::ResourceState::CopySource);
  compute_encoder->endEncoding();
  compute_encoder->release();
  command_buffer->close();

  gfx::IFence::Desc fence_desc{};
  fence_desc.initialValue = 0;
  Slang::ComPtr<gfx::IFence> fence;
  if (!result_succeeded(device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return probe;
  }

  queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;

  gfx::IFence *fences[] = {fence};
  std::uint64_t fence_values[] = {1};
  if (!result_succeeded(device->waitForFences(1, fences, fence_values, true,
                                             gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return probe;
  }
  probe.fence_completed = true;

  std::array<std::uint32_t, 3> counts{};
  if (!readback_counts(device, output_buffer, counts)) {
    probe.status = "occupancy_readback_failed";
    return probe;
  }
  probe.empty_occupancy = counts[0];
  probe.uniform_occupancy = counts[1];
  probe.mixed_occupancy = counts[2];
  probe.readback_valid = counts[0] == EmptyExpectedOccupancy &&
                         counts[1] == UniformExpectedOccupancy &&
                         counts[2] == MixedExpectedOccupancy;
  if (!probe.readback_valid) {
    probe.status = "occupancy_readback_mismatch";
    return probe;
  }

  if (!result_succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }

  probe.status = "gpu_voxel_occupancy_validated";
  return probe;
#else
  return {false, false, false, false, false, false, false, false, false,
          false, false, false, false, 0u,    0u,    0u,    "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
