#include "octaryn_client_block_atlas.h"

#include "octaryn_client_block_atlas_catalog.h"
#include "octaryn_client_block_atlas_textures.h"

#include <algorithm>

namespace octaryn::client::rendering {

bool load_client_block_atlas(SDL_GPUDevice *device, FILE *log,
                             ClientBlockAtlas &atlas) {
  atlas.device = device;
  const bool loaded =
      load_client_block_atlas_catalog_metadata(log, atlas) &&
      load_client_block_atlas_texture(
          device, log, atlas, "-color.png", "block_atlas_texture",
          ClientBlockAtlasTextureKind::color, atlas.color_texture) &&
      load_client_block_atlas_texture(
          device, log, atlas, "-normal.png", "block_atlas_normal_texture",
          ClientBlockAtlasTextureKind::material, atlas.normal_texture) &&
      load_client_block_atlas_texture(
          device, log, atlas, "-specular.png", "block_atlas_specular_texture",
          ClientBlockAtlasTextureKind::material, atlas.specular_texture) &&
      load_client_block_atlas_texture(
          device, log, atlas, "-animation.png", "block_atlas_animation_texture",
          ClientBlockAtlasTextureKind::animation, atlas.animation_texture);
  if (!loaded) {
    destroy_client_block_atlas(atlas);
  }
  return loaded;
}

void destroy_client_block_atlas(ClientBlockAtlas &atlas) {
  if (atlas.animation_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.animation_texture);
    atlas.animation_texture = nullptr;
  }
  if (atlas.specular_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.specular_texture);
    atlas.specular_texture = nullptr;
  }
  if (atlas.normal_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.normal_texture);
    atlas.normal_texture = nullptr;
  }
  if (atlas.color_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.color_texture);
    atlas.color_texture = nullptr;
  }
  atlas.device = nullptr;
}

int32_t client_block_atlas_top_layer_for_block(const ClientBlockAtlas &atlas,
                                               uint16_t block) {
  return block < atlas.block_top_layers.size() ? atlas.block_top_layers[block]
                                               : -1;
}

uint16_t
client_block_atlas_default_placeable_block(const ClientBlockAtlas &atlas,
                                           uint16_t fallback) {
  if (std::find(atlas.placeable_blocks.begin(), atlas.placeable_blocks.end(),
                fallback) != atlas.placeable_blocks.end()) {
    return fallback;
  }
  return atlas.placeable_blocks.empty() ? fallback : atlas.placeable_blocks[0];
}

uint16_t
client_block_atlas_scroll_placeable_block(const ClientBlockAtlas &atlas,
                                          uint16_t current, int delta) {
  if (delta == 0 || atlas.placeable_blocks.empty()) {
    return client_block_atlas_default_placeable_block(atlas, current);
  }

  const auto iterator = std::find(atlas.placeable_blocks.begin(),
                                  atlas.placeable_blocks.end(), current);
  const ptrdiff_t start =
      iterator == atlas.placeable_blocks.end()
          ? 0
          : std::distance(atlas.placeable_blocks.begin(), iterator);
  const ptrdiff_t count = static_cast<ptrdiff_t>(atlas.placeable_blocks.size());
  const ptrdiff_t wrapped = (start + delta) % count;
  const ptrdiff_t index = wrapped < 0 ? wrapped + count : wrapped;
  return atlas.placeable_blocks[static_cast<size_t>(index)];
}

} // namespace octaryn::client::rendering
