#include "WorldMeshTransfer.h"

#include "Log.h"

#include <cstring>
#include <limits>

namespace octaryn_client_app {

bool create_world_mesh_transfer(SDL_GPUDevice *device,
                                std::vector<SDL_GPUTransferBuffer *> &transfers,
                                const void *data, uint64_t byte_count,
                                SDL_GPUTransferBuffer *&transfer) {
  transfer = nullptr;
  if (byte_count == 0u) {
    return true;
  }
  if (byte_count > std::numeric_limits<Uint32>::max()) {
    log_line("gpu_chunk_mesh_transfer=too_large");
    return false;
  }

  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = static_cast<Uint32>(byte_count);
  transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    log_line("gpu_chunk_mesh_transfer=create_failed");
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    transfer = nullptr;
    log_line("gpu_chunk_mesh_transfer=map_failed");
    return false;
  }
  std::memcpy(mapped, data, static_cast<size_t>(byte_count));
  SDL_UnmapGPUTransferBuffer(device, transfer);
  transfers.push_back(transfer);
  return true;
}

void queue_world_mesh_transfer_upload(SDL_GPUCopyPass *copy_pass,
                                      SDL_GPUTransferBuffer *transfer,
                                      uint64_t source_offset,
                                      SDL_GPUBuffer *target,
                                      uint64_t byte_count, bool cycle) {
  if (transfer == nullptr || byte_count == 0u) {
    return;
  }
  SDL_GPUTransferBufferLocation source{};
  source.transfer_buffer = transfer;
  source.offset = static_cast<Uint32>(source_offset);
  SDL_GPUBufferRegion destination{};
  destination.buffer = target;
  destination.size = static_cast<Uint32>(byte_count);
  SDL_UploadToGPUBuffer(copy_pass, &source, &destination, cycle);
}

} // namespace octaryn_client_app
