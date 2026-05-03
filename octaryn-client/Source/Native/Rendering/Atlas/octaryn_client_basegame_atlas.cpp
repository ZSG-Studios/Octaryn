#include "octaryn_client_basegame_atlas.h"

#include "octaryn_client_asset_path.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace octaryn::client::rendering {

namespace basegame_atlas_json {

struct BlockAtlasLayers {
  int32_t north = 0;
  int32_t south = 0;
  int32_t east = 0;
  int32_t west = 0;
  int32_t up = 0;
  int32_t down = 0;
};

struct BlockCatalogEntry {
  BlockAtlasLayers atlas;
};

struct BlockCatalogFile {
  std::string schema;
  std::vector<BlockCatalogEntry> blocks;
};

} // namespace basegame_atlas_json

namespace {

constexpr int32_t kExpectedAtlasLayers = 29;
constexpr int32_t kExpectedAtlasTileSize = 32;
constexpr const char *kExpectedBlockCatalogSchema =
    "octaryn.basegame.blocks.v1";
constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};

void log_line(FILE *log, const char *message) {
  if (log != nullptr) {
    std::fprintf(log, "%s\n", message);
    std::fflush(log);
  }
}

bool read_text_file(const char *path, const char *failure_label, FILE *log,
                    std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(log, failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

int32_t manifest_int_value(const std::string &payload, const char *key) {
  const size_t offset = payload.find(key);
  if (offset == std::string::npos) {
    return -1;
  }

  const char *start =
      payload.c_str() + offset + std::char_traits<char>::length(key);
  char *end = nullptr;
  const long value = std::strtol(start, &end, 10);
  if (end == start || value < 0 || value > INT32_MAX) {
    return -1;
  }
  return static_cast<int32_t>(value);
}

bool load_basegame_atlas_manifest(FILE *log, BasegameAtlas &atlas) {
  char path[4096] = {};
  if (!octaryn_client_asset_path_build(path, sizeof(path),
                                       "Atlases/basegame-color.txt")) {
    log_line(log, "basegame_atlas_manifest_path=failed");
    return false;
  }

  std::string payload;
  if (!read_text_file(path, "basegame_atlas_manifest=open_failed", log,
                      payload)) {
    return false;
  }

  atlas.layer_count = manifest_int_value(payload, "layers=");
  atlas.tile_size = manifest_int_value(payload, "tile_size=");
  if (log != nullptr) {
    std::fprintf(log, "basegame_atlas_layers=%d\n", atlas.layer_count);
    std::fprintf(log, "basegame_atlas_tile_size=%d\n", atlas.tile_size);
    std::fflush(log);
  }
  if (atlas.layer_count != kExpectedAtlasLayers ||
      atlas.tile_size != kExpectedAtlasTileSize) {
    log_line(log, "basegame_atlas_manifest=invalid");
    return false;
  }

  log_line(log, "basegame_atlas_manifest=loaded");
  return true;
}

bool load_basegame_block_catalog(FILE *log, BasegameAtlas &atlas) {
  char path[4096] = {};
  if (!octaryn_client_bundle_path_build(
          path, sizeof(path), "Data/Blocks/octaryn.basegame.blocks.json")) {
    log_line(log, "basegame_block_catalog_path=failed");
    return false;
  }

  std::string payload;
  if (!read_text_file(path, "basegame_block_catalog=open_failed", log,
                      payload)) {
    return false;
  }

  basegame_atlas_json::BlockCatalogFile catalog{};
  const auto error = glz::read<kJsonReadOptions>(catalog, payload);
  if (error) {
    log_line(log, "basegame_block_catalog=parse_failed");
    return false;
  }

  if (catalog.schema != kExpectedBlockCatalogSchema || catalog.blocks.empty()) {
    log_line(log, "basegame_block_catalog=invalid");
    return false;
  }

  atlas.block_top_layers.clear();
  atlas.block_top_layers.reserve(catalog.blocks.size());
  for (const auto &block : catalog.blocks) {
    if (block.atlas.up < 0 || block.atlas.up >= atlas.layer_count) {
      log_line(log, "basegame_block_catalog=invalid_atlas_layer");
      return false;
    }
    atlas.block_top_layers.push_back(block.atlas.up);
  }

  if (log != nullptr) {
    std::fprintf(log, "basegame_block_catalog_entries=%zu\n",
                 atlas.block_top_layers.size());
    std::fflush(log);
  }
  log_line(log, "basegame_block_catalog=loaded");
  return true;
}

bool load_basegame_atlas_texture(SDL_Renderer *renderer, FILE *log,
                                 BasegameAtlas &atlas) {
  char path[4096] = {};
  if (!octaryn_client_asset_path_build(path, sizeof(path),
                                       "Atlases/basegame-color.png")) {
    log_line(log, "basegame_atlas_texture_path=failed");
    return false;
  }

  SDL_Surface *surface = SDL_LoadPNG(path);
  if (surface == nullptr) {
    log_line(log, "basegame_atlas_texture=open_failed");
    return false;
  }

  const int32_t expected_width = atlas.layer_count * atlas.tile_size;
  if (surface->w != expected_width || surface->h != atlas.tile_size) {
    if (log != nullptr) {
      std::fprintf(log, "basegame_atlas_texture_width=%d\n", surface->w);
      std::fprintf(log, "basegame_atlas_texture_height=%d\n", surface->h);
      std::fflush(log);
    }
    SDL_DestroySurface(surface);
    log_line(log, "basegame_atlas_texture=invalid_dimensions");
    return false;
  }

  atlas.texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_DestroySurface(surface);
  if (atlas.texture == nullptr) {
    log_line(log, "basegame_atlas_texture=create_failed");
    return false;
  }

  if (!SDL_SetTextureScaleMode(atlas.texture, SDL_SCALEMODE_NEAREST)) {
    SDL_DestroyTexture(atlas.texture);
    atlas.texture = nullptr;
    log_line(log, "basegame_atlas_texture_scale=failed");
    return false;
  }

  log_line(log, "basegame_atlas_texture=loaded");
  return true;
}

} // namespace

bool load_basegame_atlas(SDL_Renderer *renderer, FILE *log,
                         BasegameAtlas &atlas) {
  return load_basegame_atlas_manifest(log, atlas) &&
         load_basegame_block_catalog(log, atlas) &&
         load_basegame_atlas_texture(renderer, log, atlas);
}

void destroy_basegame_atlas(BasegameAtlas &atlas) {
  if (atlas.texture != nullptr) {
    SDL_DestroyTexture(atlas.texture);
    atlas.texture = nullptr;
  }
}

int32_t basegame_atlas_top_layer_for_block(const BasegameAtlas &atlas,
                                           uint16_t block) {
  return block < atlas.block_top_layers.size() ? atlas.block_top_layers[block]
                                               : -1;
}

} // namespace octaryn::client::rendering
