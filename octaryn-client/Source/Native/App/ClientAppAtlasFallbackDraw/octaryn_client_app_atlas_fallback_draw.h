#pragma once

#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_camera.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

bool draw_atlas_fallback_blocks(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::ClientBlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const octaryn_client_camera &camera, int &drawn_tiles);
bool draw_material_atlas_probe(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::ClientBlockAtlas &atlas);

} // namespace octaryn_client_app
