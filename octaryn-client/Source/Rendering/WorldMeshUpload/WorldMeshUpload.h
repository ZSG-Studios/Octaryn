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
  struct chunk_buffers {
    octaryn_client_chunk_mesh_upload_record record{};
    SDL_GPUBuffer *opaque_faces = nullptr;
    SDL_GPUBuffer *opaque_indirect = nullptr;
    SDL_GPUBuffer *transparent_faces = nullptr;
    SDL_GPUBuffer *sprite_vertices = nullptr;
    SDL_GPUBuffer *sprite_indirect = nullptr;
    uint64_t opaque_capacity = 0u;
    uint64_t opaque_indirect_capacity = 0u;
    uint64_t transparent_capacity = 0u;
    uint64_t sprite_capacity = 0u;
    uint64_t sprite_indirect_capacity = 0u;
  };

  std::vector<chunk_buffers> chunks;
  uint64_t opaque_faces = 0u;
  uint64_t transparent_faces = 0u;
  uint64_t sprite_vertices = 0u;
  uint64_t opaque_bytes = 0u;
  uint64_t transparent_bytes = 0u;
  uint64_t sprite_bytes = 0u;
};

bool drain_chunk_mesh_uploads(uint64_t frame_index,
                              world_mesh_upload_scratch &scratch,
                              world_mesh_upload_frame &upload_frame);
bool apply_world_mesh_upload_update(SDL_GPUDevice *gpu_device,
                                    world_mesh_upload_frame &visible_frame,
                                    const world_mesh_upload_frame &update_frame,
                                    world_mesh_gpu_buffers &mesh_buffers,
                                    uint64_t frame_index, const char *source,
                                    int &result);
void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index, const char *source);
void release_world_mesh_gpu_buffers(SDL_GPUDevice *device,
                                    world_mesh_gpu_buffers &buffers);
bool world_mesh_gpu_has_geometry(const world_mesh_gpu_buffers &buffers);
bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index);
