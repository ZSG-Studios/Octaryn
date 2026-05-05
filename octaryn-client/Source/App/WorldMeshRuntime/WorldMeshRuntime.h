#pragma once

#include "EmptyWorldMesh.h"
#include "WorldMeshUpload.h"

#include <SDL3/SDL.h>

#include <vector>

namespace octaryn_client_app {

struct world_mesh_runtime {
  void *handle = nullptr;
};

bool world_mesh_runtime_start(world_mesh_runtime &runtime);
void world_mesh_runtime_stop(world_mesh_runtime &runtime);
bool run_server_stream_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame, world_mesh_gpu_buffers &mesh_buffers,
    const server_chunk_stream_file &stream, const block_lookup &block_lookup,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    uint64_t frame_index, const char *source, int &result);
bool run_empty_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame, world_mesh_gpu_buffers &mesh_buffers,
    const chunk_view &current_view, const chunk_view &previous_chunk_view,
    const block_lookup &block_lookup, uint64_t frame_index, const char *source,
    int &result);
} // namespace octaryn_client_app
