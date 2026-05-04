#pragma once

#include "BlockAtlas.h"

#include <SDL3/SDL.h>

namespace octaryn_client_app {

bool load_empty_world_atlas(SDL_GPUDevice *device,
                            octaryn::client::rendering::BlockAtlas &atlas);

} // namespace octaryn_client_app
