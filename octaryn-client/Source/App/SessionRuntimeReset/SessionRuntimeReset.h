#pragma once

#include "ChunkView.h"
#include "PresentationSnapshots.h"
#include "PresentationState.h"
#include "WorldMeshRuntime.h"
#include "WorldMeshUpload.h"
#include "WorldStream.h"

#include <SDL3/SDL.h>

#include <vector>

namespace octaryn_client_app {

bool reset_session_runtime_state(
    SDL_GPUDevice *gpu_device, world_mesh_gpu_buffers &mesh_buffers,
    world_mesh_runtime &mesh_runtime, world_mesh_upload_frame &visible_frame,
    client_server_stream_poll_state &stream_poll,
    std::vector<presentation_block> &presentation_blocks,
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks, block_lookup &lookup,
    chunk_view &mesh_chunk_view, chunk_view &logged_chunk_view,
    server_world_time_state &world_time, int &result);

} // namespace octaryn_client_app
