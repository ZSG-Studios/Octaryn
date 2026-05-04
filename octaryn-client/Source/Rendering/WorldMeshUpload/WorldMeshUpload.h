#pragma once

#include "HostExports.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

constexpr uint32_t kMaxChunkMeshUploadsPerFrame = 4096u;
constexpr uint32_t kMaxPackedOpaqueFacesPerFrame = 8388608u;
constexpr uint32_t kMaxPackedTransparentFacesPerFrame = 1048576u;
constexpr uint32_t kMaxPackedSpriteVerticesPerFrame = 4194304u;

struct world_mesh_upload_frame {
  std::vector<octaryn_client_chunk_mesh_upload_record> chunks;
  std::vector<uint64_t> opaque_faces;
  std::vector<uint64_t> transparent_faces;
  std::vector<uint32_t> sprite_vertices;
  uint32_t fluid_blocks = 0u;
  uint64_t opaque_bytes = 0u;
  uint64_t transparent_bytes = 0u;
  uint64_t sprite_bytes = 0u;
};

struct world_mesh_upload_scratch {
  std::vector<octaryn_client_chunk_mesh_upload_record> chunks;
  std::vector<uint64_t> opaque_faces;
  std::vector<uint64_t> transparent_faces;
  std::vector<uint32_t> sprite_vertices;
};

struct world_mesh_gpu_buffers {
  SDL_GPUBuffer *opaque_faces = nullptr;
  SDL_GPUBuffer *transparent_faces = nullptr;
  SDL_GPUBuffer *sprite_vertices = nullptr;
};

bool drain_chunk_mesh_uploads(uint64_t frame_index,
                              world_mesh_upload_scratch &scratch,
                              world_mesh_upload_frame &upload_frame);
void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index);
void release_world_mesh_gpu_buffers(SDL_GPUDevice *device,
                                    world_mesh_gpu_buffers &buffers);
bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index);
