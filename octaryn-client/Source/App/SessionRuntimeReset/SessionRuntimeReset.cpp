#include "SessionRuntimeReset.h"

#include <limits>

namespace octaryn_client_app {

bool reset_session_runtime_state(
    SDL_GPUDevice *gpu_device, world_mesh_gpu_buffers &mesh_buffers,
    world_mesh_runtime &mesh_runtime, world_mesh_upload_frame &visible_frame,
    client_server_stream_poll_state &stream_poll,
    std::vector<presentation_block> &presentation_blocks,
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks, block_lookup &lookup,
    chunk_view &mesh_chunk_view, chunk_view &logged_chunk_view,
    server_world_time_state &world_time, int &result) {
  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  mesh_buffers = {};
  visible_frame = {};
  world_mesh_runtime_stop(mesh_runtime);
  mesh_runtime = {};
  if (!world_mesh_runtime_start(mesh_runtime)) {
    result = -10;
    return false;
  }

  stream_poll = {};
  stream_poll.active_server_stream_override_signature =
      std::numeric_limits<uint64_t>::max();
  presentation_blocks.clear();
  snapshot_blocks.clear();
  surface_blocks.clear();
  lookup.clear();
  mesh_chunk_view = {std::numeric_limits<int>::min(),
                     std::numeric_limits<int>::min(), 0};
  logged_chunk_view = mesh_chunk_view;
  world_time = {};
  return true;
}

} // namespace octaryn_client_app
