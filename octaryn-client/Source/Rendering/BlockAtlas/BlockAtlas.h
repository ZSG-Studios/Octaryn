#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace octaryn::client::rendering {

struct BlockAtlas {
  SDL_GPUDevice *device = nullptr;
  SDL_GPUTexture *color_texture = nullptr;
  SDL_GPUTexture *normal_texture = nullptr;
  SDL_GPUTexture *specular_texture = nullptr;
  SDL_GPUTexture *animation_texture = nullptr;
  int32_t layer_count = 0;
  int32_t tile_size = 0;
  int32_t animation_frames = 0;
  int32_t animation_count = 0;
  std::vector<int32_t> block_top_layers;
  std::vector<uint16_t> placeable_blocks;
};

bool load_block_atlas(SDL_GPUDevice *device, FILE *log, BlockAtlas &atlas);
void destroy_block_atlas(BlockAtlas &atlas);
int32_t block_atlas_top_layer_for_block(const BlockAtlas &atlas,
                                        uint16_t block);
uint16_t block_atlas_default_placeable_block(const BlockAtlas &atlas,
                                             uint16_t fallback);
uint16_t block_atlas_scroll_placeable_block(const BlockAtlas &atlas,
                                            uint16_t current, int delta);

} // namespace octaryn::client::rendering
