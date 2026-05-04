#pragma once

#include "PresentationState.h"
#include "BlockAtlas.h"
#include "Camera.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

bool draw_atlas_fallback_blocks(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const camera &camera, int &drawn_tiles);
bool draw_material_atlas_probe(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::BlockAtlas &atlas);

} // namespace octaryn_client_app
