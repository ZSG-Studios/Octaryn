#pragma once

#include "BlockInteraction.h"
#include "PresentationState.h"
#include "ShaderPipelines.h"
#include "WorldStream.h"
#include "BlockAtlas.h"
#include "Camera.h"
#include "FrameProfile.h"
#include "RuntimeControls.h"
#include "WorldMeshUpload.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

bool present_frame(
    SDL_GPUDevice *device, SDL_Window *window,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const camera &camera,
    const client_block_raycast_hit &selection_hit, uint16_t selected_place_block,
    const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const server_world_time_state &world_time,
    const runtime_controls &controls,
    const frame_profile_snapshot &profile, uint64_t frame_index,
    frame_profile_sample *profile_sample);

} // namespace octaryn_client_app
