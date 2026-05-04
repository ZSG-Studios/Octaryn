#pragma once

#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_app_shader_pipelines.h"
#include "octaryn_client_app_world_stream.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_swapchain.h"
#include "octaryn_singleplayer_server_session.h"

#include <SDL3/SDL.h>

#include <vector>

namespace octaryn_client_app {

int run_client_app_frame_loop(
    SDL_GPUDevice *gpu_device, SDL_Window *window,
    const octaryn::client::rendering::ClientBlockAtlas &atlas,
    bool game_modules_disabled, singleplayer_server_session &server_session,
    octaryn_client_frame_pacing &frame_pacing,
    octaryn_client_swapchain_state &swapchain_state,
    client_shader_pipelines &shader_pipelines,
    std::vector<presentation_block> &world_snapshot_blocks,
    std::vector<presentation_block> &world_surface_blocks,
    server_world_time_state &world_time, block_lookup &world_block_lookup);

} // namespace octaryn_client_app
