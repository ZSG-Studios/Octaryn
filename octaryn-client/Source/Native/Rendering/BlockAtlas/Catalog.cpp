#include "Catalog.h"

#include "BundleFile.h"

#include <glaze/glaze.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace octaryn::client::rendering::block_atlas_json {

struct BlockAtlasLayers {
  int32_t north = 0;
  int32_t south = 0;
  int32_t east = 0;
  int32_t west = 0;
  int32_t up = 0;
  int32_t down = 0;
};

struct BlockCatalogEntry {
  bool placeable = false;
  BlockAtlasLayers atlas;
};

struct BlockCatalogFile {
  std::string schema;
  std::vector<BlockCatalogEntry> blocks;
};

} // namespace octaryn::client::rendering::block_atlas_json

namespace octaryn::client::rendering {

namespace {

constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};

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

bool load_block_atlas_manifest(FILE *log, BlockAtlas &atlas) {
  std::string path;
  if (!find_first_block_atlas_bundle_file(
          "Assets/Atlases", "-color.txt", "block_atlas_manifest_path=failed",
          log, path)) {
    return false;
  }

  std::string payload;
  if (!read_block_atlas_text_file(
          path.c_str(), "block_atlas_manifest=open_failed", log, payload)) {
    return false;
  }

  atlas.layer_count = manifest_int_value(payload, "layers=");
  atlas.tile_size = manifest_int_value(payload, "tile_size=");
  if (log != nullptr) {
    std::fprintf(log, "block_atlas_layers=%d\n", atlas.layer_count);
    std::fprintf(log, "block_atlas_tile_size=%d\n", atlas.tile_size);
    std::fflush(log);
  }
  if (atlas.layer_count <= 0 || atlas.tile_size <= 0) {
    log_block_atlas_line(log, "block_atlas_manifest=invalid");
    return false;
  }

  log_block_atlas_line(log, "block_atlas_manifest=loaded");
  return true;
}

bool load_block_animation_manifest(FILE *log, BlockAtlas &atlas) {
  std::string path;
  if (!find_first_block_atlas_bundle_file(
          "Assets/Atlases", "-animation.txt",
          "block_animation_manifest_path=failed", log, path)) {
    return false;
  }

  std::string payload;
  if (!read_block_atlas_text_file(
          path.c_str(), "block_animation_manifest=open_failed", log, payload)) {
    return false;
  }

  const int32_t animation_tile_size = manifest_int_value(payload, "tile_size=");
  atlas.animation_frames = manifest_int_value(payload, "frames=");
  atlas.animation_count = manifest_int_value(payload, "animations=");
  if (log != nullptr) {
    std::fprintf(log, "block_animation_tile_size=%d\n", animation_tile_size);
    std::fprintf(log, "block_animation_frames=%d\n", atlas.animation_frames);
    std::fprintf(log, "block_animation_count=%d\n", atlas.animation_count);
    std::fflush(log);
  }

  if (animation_tile_size != atlas.tile_size || atlas.animation_frames < 0 ||
      atlas.animation_count < 0) {
    log_block_atlas_line(log, "block_animation_manifest=invalid");
    return false;
  }

  log_block_atlas_line(log, "block_animation_manifest=loaded");
  return true;
}

bool load_block_catalog(FILE *log, BlockAtlas &atlas) {
  std::string path;
  if (!find_first_block_atlas_bundle_file("Data/Blocks", ".blocks.json",
                                          "block_catalog_path=failed", log,
                                          path)) {
    return false;
  }

  std::string payload;
  if (!read_block_atlas_text_file(
          path.c_str(), "block_catalog=open_failed", log, payload)) {
    return false;
  }

  block_atlas_json::BlockCatalogFile catalog{};
  const auto error = glz::read<kJsonReadOptions>(catalog, payload);
  if (error) {
    log_block_atlas_line(log, "block_catalog=parse_failed");
    return false;
  }

  if (!block_atlas_ends_with(catalog.schema, ".blocks.v1") ||
      catalog.blocks.empty()) {
    log_block_atlas_line(log, "block_catalog=invalid");
    return false;
  }

  atlas.block_top_layers.clear();
  atlas.placeable_blocks.clear();
  atlas.block_top_layers.reserve(catalog.blocks.size());
  atlas.placeable_blocks.reserve(catalog.blocks.size());
  for (size_t index = 0; index < catalog.blocks.size(); ++index) {
    const auto &block = catalog.blocks[index];
    if (block.atlas.up < 0 || block.atlas.up >= atlas.layer_count) {
      log_block_atlas_line(log, "block_catalog=invalid_atlas_layer");
      return false;
    }
    atlas.block_top_layers.push_back(block.atlas.up);
    if (block.placeable && index <= UINT16_MAX) {
      atlas.placeable_blocks.push_back(static_cast<uint16_t>(index));
    }
  }

  if (log != nullptr) {
    std::fprintf(log, "block_catalog_entries=%zu\n",
                 atlas.block_top_layers.size());
    std::fprintf(log, "block_atlas_placeable_blocks=%zu\n",
                 atlas.placeable_blocks.size());
    std::fflush(log);
  }
  log_block_atlas_line(log, "block_catalog=loaded");
  return true;
}

} // namespace

bool load_block_atlas_catalog_metadata(FILE *log, BlockAtlas &atlas) {
  return load_block_atlas_manifest(log, atlas) &&
         load_block_animation_manifest(log, atlas) &&
         load_block_catalog(log, atlas);
}

} // namespace octaryn::client::rendering
