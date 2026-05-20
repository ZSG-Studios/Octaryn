#pragma once

#include "EmptyWorldMesh.h"
#include "WorldMeshUpload.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

namespace octaryn_client_app {

struct server_stream_mesh_batch_state {
  bool active = false;
  uint64_t epoch = 0u;
  chunk_view previous_view{};
  chunk_view target_view{};
  size_t next_plan_entry = 0u;
  size_t batch_index = 0u;
};

struct world_mesh_runtime {
  void *handle = nullptr;
  server_stream_mesh_batch_state server_stream_batch{};
  void *server_stream_pending = nullptr;
};

bool world_mesh_runtime_start(world_mesh_runtime &runtime);
void world_mesh_runtime_stop(world_mesh_runtime &runtime);
bool run_server_stream_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers,
    const server_chunk_stream_file &stream, const block_lookup &block_lookup,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    uint64_t frame_index, const char *source, int &result);
bool run_empty_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers, const chunk_view &current_view,
    const chunk_view &previous_chunk_view, const block_lookup &block_lookup,
    uint64_t frame_index, const char *source, int &result);
bool run_frame_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers, bool game_modules_disabled,
    bool server_session_enabled, bool has_server_stream,
    bool server_stream_mesh_dirty, bool empty_world_local_edit,
    const server_chunk_stream_file &server_stream,
    const std::vector<empty_world_dirty_column> &server_stream_dirty_columns,
    const chunk_view &current_chunk_view, chunk_view &mesh_chunk_view,
    const block_lookup &block_lookup, uint64_t frame_index, int &result);
} // namespace octaryn_client_app
