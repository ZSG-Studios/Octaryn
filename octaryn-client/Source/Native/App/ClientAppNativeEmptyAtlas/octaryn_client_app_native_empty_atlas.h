#pragma once

#include "octaryn_client_block_atlas.h"

#include <SDL3/SDL.h>

namespace octaryn_client_app {

bool load_native_empty_world_atlas(
    SDL_GPUDevice *device, octaryn::client::rendering::ClientBlockAtlas &atlas);

} // namespace octaryn_client_app
