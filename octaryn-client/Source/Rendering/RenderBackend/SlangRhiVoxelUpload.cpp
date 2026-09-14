#include "SlangRhiVoxelUpload.h"

#include "ChunkPalette.h"
#include "GpuChunkPayload.h"

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
using octaryn::client::voxel::GpuChunkHeader;
using octaryn::client::voxel::GpuChunkPaletteEntry;
using octaryn::client::voxel::GpuChunkPayload;

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

struct VoxelUploadBatch {
  std::vector<GpuChunkHeader> headers;
  std::vector<GpuChunkPaletteEntry> palette_entries;
  std::vector<std::uint8_t> payload_bytes;
  std::uint32_t empty_header_index;
  std::uint32_t uniform_header_index;
  std::uint32_t mixed_header_index;
};

std::vector<std::uint8_t> build_mixed_payload_bytes() {
  const std::uint8_t bits_per_voxel = 2u;
  const std::uint32_t packed_bytes =
      octaryn::client::voxel::packed_voxel_payload_bytes(bits_per_voxel);
  std::vector<std::uint8_t> bytes;
  bytes.reserve(packed_bytes);
  for (std::uint32_t index = 0; index < packed_bytes; ++index) {
    bytes.push_back(static_cast<std::uint8_t>((index * 37u + 19u) & 0xffu));
  }
  return bytes;
}

GpuChunkPayload build_mixed_payload() {
  const std::array<GpuChunkPaletteEntry, 4> entries = {
      GpuChunkPaletteEntry{11u, 110u, 1u, 0u},
      GpuChunkPaletteEntry{12u, 120u, 1u, 0u},
      GpuChunkPaletteEntry{13u, 130u, 1u, 0u},
      GpuChunkPaletteEntry{14u, 140u, 1u, 0u},
  };
  const auto payload = build_mixed_payload_bytes();
  return octaryn::client::voxel::make_mixed_gpu_chunk(
      5, 2, entries.data(), static_cast<std::uint32_t>(entries.size()),
      payload.data(), static_cast<std::uint32_t>(payload.size()));
}

bool append_chunk(VoxelUploadBatch &batch, GpuChunkPayload chunk) {
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

VoxelUploadBatch build_upload_batch() {
  VoxelUploadBatch batch{{}, {}, {}, 0u, 1u, 2u};
  append_chunk(batch, octaryn::client::voxel::make_empty_gpu_chunk(0, 0));
  append_chunk(batch,
               octaryn::client::voxel::make_uniform_gpu_chunk(1, 0, 42u));
  append_chunk(batch, build_mixed_payload());
  return batch;
}

bool create_upload_buffer(gfx::IDevice *device,
                          const std::vector<std::uint8_t> &payload,
                          Slang::ComPtr<gfx::IBufferResource> &buffer) {
  gfx::IBufferResource::Desc buffer_desc{};
  buffer_desc.type = gfx::IResource::Type::Buffer;
  buffer_desc.defaultState = gfx::ResourceState::CopyDestination;
  buffer_desc.allowedStates = gfx::ResourceStateSet(
      gfx::ResourceState::CopyDestination, gfx::ResourceState::CopySource);
  buffer_desc.memoryType = gfx::MemoryType::DeviceLocal;
  buffer_desc.sizeInBytes = payload.size();
  buffer_desc.elementSize = sizeof(std::uint8_t);
  return result_succeeded(
             device->createBufferResource(buffer_desc, nullptr,
                                          buffer.writeRef())) &&
         buffer != nullptr;
}

bool upload_payload(gfx::IResourceCommandEncoder *encoder,
                    gfx::IBufferResource *buffer,
                    const std::vector<std::uint8_t> &payload) {
  encoder->uploadBufferData(buffer, 0, payload.size(),
                            const_cast<std::uint8_t *>(payload.data()));
  encoder->bufferBarrier(buffer, gfx::ResourceState::CopyDestination,
                         gfx::ResourceState::CopySource);
  return true;
}

bool readback_matches(gfx::IDevice *device, gfx::IBufferResource *buffer,
                      const std::vector<std::uint8_t> &payload) {
  Slang::ComPtr<ISlangBlob> readback;
  if (!result_succeeded(device->readBufferResource(
          buffer, 0, payload.size(), readback.writeRef())) ||
      readback == nullptr) {
    return false;
  }
  return readback->getBufferSize() == payload.size() &&
         std::memcmp(readback->getBufferPointer(), payload.data(),
                     payload.size()) == 0;
}

} // namespace
#endif

SlangRhiVoxelUploadProbeResult probe_slang_rhi_voxel_palette_uploads() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  SlangRhiVoxelUploadProbeResult probe{true,  false, false, false, false,
                                       false, false, false, false, false,
                                       false, false, false, false, false,
                                       false, "not_started"};

  const VoxelUploadBatch batch = build_upload_batch();
  if (batch.headers.size() != 3u || batch.palette_entries.size() != 5u ||
      batch.payload_bytes.empty()) {
    probe.status = "upload_batch_invalid";
    return probe;
  }
  const std::vector<std::uint8_t> headers = serialize_values(batch.headers);
  const std::vector<std::uint8_t> palette =
      serialize_values(batch.palette_entries);
  const std::vector<std::uint8_t> payload = batch.payload_bytes;

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
  heap_desc.uavDescriptorCount = 8;
  heap_desc.constantBufferDescriptorCount = 8;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!result_succeeded(device->createTransientResourceHeap(heap_desc,
                                                           heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  if (!result_succeeded(heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return probe;
  }

  Slang::ComPtr<gfx::IBufferResource> header_buffer;
  Slang::ComPtr<gfx::IBufferResource> palette_buffer;
  Slang::ComPtr<gfx::IBufferResource> payload_buffer;
  if (!create_upload_buffer(device, headers, header_buffer) ||
      !create_upload_buffer(device, palette, palette_buffer) ||
      !create_upload_buffer(device, payload, payload_buffer)) {
    probe.status = "buffer_create_failed";
    return probe;
  }

  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!result_succeeded(heap->createCommandBuffer(command_buffer.writeRef())) ||
      command_buffer == nullptr) {
    probe.status = "command_buffer_create_failed";
    return probe;
  }
  probe.command_buffer_created = true;

  gfx::IResourceCommandEncoder *encoder = nullptr;
  command_buffer->encodeResourceCommands(&encoder);
  if (encoder == nullptr) {
    probe.status = "resource_encoder_create_failed";
    return probe;
  }

  probe.empty_uploaded =
      octaryn::client::voxel::gpu_chunk_payload_valid(
          octaryn::client::voxel::make_empty_gpu_chunk(0, 0));
  probe.uniform_uploaded =
      batch.headers[batch.uniform_header_index].uniform_block_id == 42u;
  probe.mixed_uploaded =
      batch.headers[batch.mixed_header_index].palette_offset == 1u &&
      batch.headers[batch.mixed_header_index].voxel_data_offset == 0u;
  upload_payload(encoder, header_buffer, headers);
  upload_payload(encoder, palette_buffer, palette);
  upload_payload(encoder, payload_buffer, payload);
  encoder->endEncoding();
  encoder->release();

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

  probe.headers_readback_valid =
      readback_matches(device, header_buffer, headers);
  probe.palette_readback_valid =
      readback_matches(device, palette_buffer, palette);
  probe.payload_readback_valid =
      readback_matches(device, payload_buffer, payload);
  probe.empty_readback_valid =
      batch.headers[batch.empty_header_index].flags == ChunkPaletteEmpty &&
      batch.headers[batch.empty_header_index].palette_offset == 0u &&
      batch.headers[batch.empty_header_index].voxel_data_offset == 0u;
  probe.uniform_readback_valid =
      batch.headers[batch.uniform_header_index].flags == ChunkPaletteUniform &&
      batch.headers[batch.uniform_header_index].palette_offset == 0u &&
      batch.headers[batch.uniform_header_index].uniform_block_id == 42u;
  probe.mixed_readback_valid =
      batch.headers[batch.mixed_header_index].flags == ChunkPaletteMixed &&
      batch.headers[batch.mixed_header_index].palette_offset == 1u &&
      batch.headers[batch.mixed_header_index].voxel_data_offset == 0u;
  if (!probe.headers_readback_valid || !probe.palette_readback_valid ||
      !probe.payload_readback_valid || !probe.empty_readback_valid ||
      !probe.uniform_readback_valid || !probe.mixed_readback_valid) {
    probe.status = "gpu_chunk_payload_readback_mismatch";
    return probe;
  }

  if (!result_succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }

  probe.status = "gpu_chunk_payload_uploads_validated";
  return probe;
#else
  return SlangRhiVoxelUploadProbeResult{
      false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
