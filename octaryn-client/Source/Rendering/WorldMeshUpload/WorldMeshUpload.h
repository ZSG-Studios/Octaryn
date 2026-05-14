#pragma once

#include "HostExports.h"
#include "RenderSection.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct world_mesh_upload_frame {
  std::vector<octaryn_client_chunk_mesh_upload_record> chunks;
  std::vector<world_render_section_state> sections;
  std::unordered_map<world_render_section_key, size_t,
                     world_render_section_key_hash>
      chunk_indices;
  std::vector<uint64_t> opaque_faces;
  std::vector<uint64_t> transparent_faces;
  std::vector<uint32_t> sprite_vertices;
  uint32_t fluid_blocks = 0u;
  uint64_t opaque_bytes = 0u;
  uint64_t transparent_bytes = 0u;
  uint64_t sprite_bytes = 0u;
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
  std::vector<world_render_section_state> sections;
  std::unordered_map<world_render_section_key, size_t,
                     world_render_section_key_hash>
      section_indices;
  std::unordered_map<world_render_section_key, size_t,
                     world_render_section_key_hash>
      chunk_indices;
  uint64_t opaque_faces = 0u;
  uint64_t transparent_faces = 0u;
  uint64_t sprite_vertices = 0u;
  uint64_t opaque_bytes = 0u;
  uint64_t transparent_bytes = 0u;
  uint64_t sprite_bytes = 0u;
};

bool apply_world_mesh_upload_update(SDL_GPUDevice *gpu_device,
                                    world_mesh_upload_frame &visible_frame,
                                    const world_mesh_upload_frame &update_frame,
                                    world_mesh_gpu_buffers &mesh_buffers,
                                    uint64_t frame_index, const char *source,
                                    int &result);
void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index, const char *source);
void rebuild_world_mesh_draw_indices(world_mesh_gpu_buffers &buffers);
void apply_world_mesh_draw_index_update(
    world_mesh_gpu_buffers &buffers,
    const world_mesh_upload_frame &update_frame);
void release_world_mesh_gpu_buffers(SDL_GPUDevice *device,
                                    world_mesh_gpu_buffers &buffers);
bool world_mesh_gpu_has_geometry(const world_mesh_gpu_buffers &buffers);
bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index);
