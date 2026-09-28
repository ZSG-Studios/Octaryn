#include "TileSet.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <glaze/glaze.hpp>

namespace octaryn::client::app {

// Named at namespace scope: clang requires external linkage for the glaze
// reflection variable instantiated from a struct with reflection.
struct TileManifestFile {
  int version{};
  std::string map;
  std::vector<std::array<float, 6>> tiles;
  std::vector<std::string> tile_files;
  std::string texture_cache;
};

namespace {

float distance_to_bounds(const WorldTile& tile, float x, float y, float z) {
  const float dx = std::max(std::max(tile.bounds[0] - x, x - tile.bounds[3]), 0.0f);
  const float dy = std::max(std::max(tile.bounds[1] - y, y - tile.bounds[4]), 0.0f);
  const float dz = std::max(std::max(tile.bounds[2] - z, z - tile.bounds[5]), 0.0f);
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

bool TileSet::load(const std::filesystem::path& manifest_path) {
  std::ifstream input(manifest_path, std::ios::binary);
  if (!input) {
    std::fprintf(stderr, "Tile manifest unreadable: %s\n", manifest_path.generic_string().c_str());
    return false;
  }
  std::error_code io_error;
  const auto bytes=std::filesystem::file_size(manifest_path,io_error);
  if(io_error || !bytes || bytes>1024u*1024u)return false;
  std::string text((std::istreambuf_iterator<char>{input}), {});
  TileManifestFile parsed;
  constexpr glz::opts options{.error_on_unknown_keys = false, .error_on_missing_keys = false};
  if (glz::read<options>(parsed, text) || parsed.version != 1 || parsed.map.empty()) {
    std::fprintf(stderr, "Tile manifest invalid: %s\n", manifest_path.generic_string().c_str());
    return false;
  }
  if(parsed.tiles.size()>65536)return false;
  directory_ = manifest_path.parent_path();
  texture_cache_.clear();
  if(!parsed.texture_cache.empty()) {
    const auto relative=std::filesystem::u8path(parsed.texture_cache);
    if(relative.is_absolute() || relative.has_root_name() ||
        std::any_of(relative.begin(),relative.end(),[](const auto& part){return part=="..";}))return false;
    texture_cache_=directory_/relative;
  }
  tiles_.clear();
  if (parsed.tiles.empty() != parsed.tile_files.empty() ||
      parsed.tiles.size() != parsed.tile_files.size()) {
    std::fprintf(stderr, "Tile manifest requires matching tiles/tile_files arrays\n");
    return false;
  }
  if (parsed.tiles.empty()) {
    // Single implicit tile: the monolithic map payload with unknown bounds.
    WorldTile& tile = tiles_.emplace_back();
    tile.file = parsed.map;
    tile.bounds[0] = tile.bounds[1] = tile.bounds[2] = -1e9f;
    tile.bounds[3] = tile.bounds[4] = tile.bounds[5] = 1e9f;
  } else {
    for (std::size_t index = 0; index < parsed.tiles.size(); ++index) {
      WorldTile& tile = tiles_.emplace_back();
      tile.file = parsed.tile_files[index];
      bool finite = true;
      for (const float value : parsed.tiles[index]) finite &= std::isfinite(value);
      const std::filesystem::path relative=std::filesystem::u8path(tile.file);
      const bool escapes=relative.is_absolute() || relative.has_root_name() ||
          std::any_of(relative.begin(),relative.end(),[](const auto& part){return part=="..";});
      const bool ordered=parsed.tiles[index][0]<=parsed.tiles[index][3] &&
          parsed.tiles[index][1]<=parsed.tiles[index][4] && parsed.tiles[index][2]<=parsed.tiles[index][5];
      if (tile.file.empty() || escapes || !finite || !ordered) {
        std::fprintf(stderr, "Tile manifest entry %zu invalid\n", index);
        return false;
      }
      for (int axis = 0; axis < 6; ++axis) tile.bounds[axis] = parsed.tiles[index][axis];
    }
  }
  return true;
}

const WorldTile* TileSet::tile(std::uint32_t index) const {
  return index < tiles_.size() ? &tiles_[index] : nullptr;
}

WorldTile* TileSet::mutable_tile(std::uint32_t index) {
  return index < tiles_.size() ? &tiles_[index] : nullptr;
}

TileResidency TileSet::evaluate(float camera_x, float camera_y, float camera_z,
                                float load_radius, float keep_radius) const {
  TileResidency result;
  std::vector<std::pair<float, std::uint32_t>> ordered;
  ordered.reserve(tiles_.size());
  for (std::uint32_t index = 0; index < tiles_.size(); ++index) {
    const auto distance = distance_to_bounds(tiles_[index], camera_x, camera_y, camera_z);
    if (distance < load_radius) ordered.emplace_back(distance, index);
    if (tiles_[index].resident && distance >= keep_radius) ++result.evict_queue;
    if (tiles_[index].resident) ++result.resident;
  }
  std::sort(ordered.begin(), ordered.end());
  result.wanted = static_cast<std::uint32_t>(ordered.size());
  result.nearest_distance = ordered.empty() ? -1.0f : ordered.front().first;
  for (const auto& [distance, index] : ordered)
    if (!tiles_[index].resident) ++result.load_queue;
  return result;
}

}
