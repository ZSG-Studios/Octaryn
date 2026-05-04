#pragma once

#include "BlockAtlas.h"

namespace octaryn::client::rendering {

enum class TextureKind {
  color,
  material,
  animation,
};

bool load_block_atlas_texture(SDL_GPUDevice *device, FILE *log,
                              BlockAtlas &atlas, const char *filename_suffix,
                              const char *log_prefix, TextureKind kind,
                              SDL_GPUTexture *&texture);

} // namespace octaryn::client::rendering
