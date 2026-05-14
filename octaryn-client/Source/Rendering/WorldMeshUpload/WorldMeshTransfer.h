#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

bool create_world_mesh_transfer(SDL_GPUDevice *device,
                                std::vector<SDL_GPUTransferBuffer *> &transfers,
                                const void *data, uint64_t byte_count,
                                SDL_GPUTransferBuffer *&transfer);
void queue_world_mesh_transfer_upload(SDL_GPUCopyPass *copy_pass,
                                      SDL_GPUTransferBuffer *transfer,
                                      uint64_t source_offset,
                                      SDL_GPUBuffer *target,
                                      uint64_t byte_count, bool cycle);

} // namespace octaryn_client_app
