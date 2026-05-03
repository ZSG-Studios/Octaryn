#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace octaryn::client::rendering {

struct BasegameAtlas {
  SDL_Texture *texture = nullptr;
  int32_t layer_count = 0;
  int32_t tile_size = 0;
  std::vector<int32_t> block_top_layers;
};

bool load_basegame_atlas(SDL_Renderer *renderer, FILE *log,
                         BasegameAtlas &atlas);
void destroy_basegame_atlas(BasegameAtlas &atlas);
int32_t basegame_atlas_top_layer_for_block(const BasegameAtlas &atlas,
                                           uint16_t block);

} // namespace octaryn::client::rendering
