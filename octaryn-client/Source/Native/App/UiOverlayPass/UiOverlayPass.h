#pragma once

#include "ShaderPipelines.h"
#include "BlockAtlas.h"
#include "octaryn_client_frame_profile.h"
#include "octaryn_client_runtime_controls.h"

#include <SDL3/SDL.h>
#include <cstdint>

namespace octaryn_client_app {

bool render_ui_overlay(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const client_shader_pipelines &pipelines,
    const octaryn_client_runtime_controls &controls,
    const octaryn_client_frame_profile_snapshot &profile,
    uint16_t selected_place_block, uint32_t target_width,
    uint32_t target_height);

} // namespace octaryn_client_app
