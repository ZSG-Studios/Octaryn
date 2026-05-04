#pragma once

#include "octaryn_client_block_atlas.h"

namespace octaryn::client::rendering {

enum class ClientBlockAtlasTextureKind {
  color,
  material,
  animation,
};

bool load_client_block_atlas_texture(SDL_GPUDevice *device, FILE *log,
                                     ClientBlockAtlas &atlas,
                                     const char *filename_suffix,
                                     const char *log_prefix,
                                     ClientBlockAtlasTextureKind kind,
                                     SDL_GPUTexture *&texture);

} // namespace octaryn::client::rendering
