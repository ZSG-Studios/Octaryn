#pragma once

#include "octaryn_client_app_block_interaction.h"
#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_app_shader_pipelines.h"
#include "octaryn_client_app_world_stream.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_frame_profile.h"
#include "octaryn_client_runtime_controls.h"
#include "octaryn_client_world_mesh_upload.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

bool present_frame(
    SDL_GPUDevice *device, SDL_Window *window,
    const octaryn::client::rendering::ClientBlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const octaryn_client_camera &camera,
    const client_block_raycast_hit &selection_hit, uint16_t selected_place_block,
    const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const server_world_time_state &world_time,
    const octaryn_client_runtime_controls &controls,
    const octaryn_client_frame_profile_snapshot &profile, uint64_t frame_index,
    octaryn_client_frame_profile_sample *profile_sample);

} // namespace octaryn_client_app
