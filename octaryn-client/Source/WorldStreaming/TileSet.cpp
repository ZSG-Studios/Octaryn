#include "TileSet.h"
#include "TileCatalogFiles.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <glaze/glaze.hpp>

namespace octaryn::client::app {

// Named at namespace scope: clang requires external linkage for the glaze
// reflection variable instantiated from a struct with reflection.
struct TileManifestFile {
  int version{};
  std::uint32_t tile_gpu_budget_mib{};
  std::string map;
  std::vector<std::array<float, 6>> tiles;
  std::vector<std::string> tile_files;
  std::vector<std::string> tile_ids;
  std::vector<bool> tile_collision;
  std::vector<std::array<std::uint64_t,2>> tile_ranges;
  std::vector<std::string> tile_catalogs;
  std::string texture_cache;
  std::string tile_residency;
  std::vector<std::uint32_t> tile_initial_wanted;
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
  // Match the host scene descriptor's bounded metadata admission. Geometry
  // preparation and GPU residency remain independently bounded per tile.
  if(io_error || !bytes || bytes>4u*1024u*1024u)return false;
  std::string text((std::istreambuf_iterator<char>{input}), {});
  TileManifestFile parsed;
  constexpr glz::opts options{.error_on_unknown_keys = false, .error_on_missing_keys = false};
  if (glz::read<options>(parsed, text) || parsed.version != 1 || parsed.map.empty()) {
    std::fprintf(stderr, "Tile manifest invalid: %s\n", manifest_path.generic_string().c_str());
    return false;
  }
  if(!parsed.tile_catalogs.empty()) {
    content::TileCatalogArrays arrays{parsed.version,std::move(parsed.tiles),std::move(parsed.tile_files),
        std::move(parsed.tile_ids),std::move(parsed.tile_collision),std::move(parsed.tile_ranges)};
    if(!content::expand_tile_catalogs(manifest_path,parsed.tile_catalogs,arrays))return false;
    parsed.tiles=std::move(arrays.tiles);parsed.tile_files=std::move(arrays.tile_files);
    parsed.tile_ids=std::move(arrays.tile_ids);parsed.tile_collision=std::move(arrays.tile_collision);
    parsed.tile_ranges=std::move(arrays.tile_ranges);
  }
  if(parsed.tile_gpu_budget_mib && (parsed.tile_gpu_budget_mib<64 || parsed.tile_gpu_budget_mib>32768))return false;
  gpu_budget_mib_=parsed.tile_gpu_budget_mib;
  if(parsed.tiles.size()>65536 || (!parsed.tile_ids.empty() && parsed.tile_ids.size()!=parsed.tiles.size()))return false;
  if(!parsed.tile_collision.empty() && parsed.tile_collision.size()!=parsed.tiles.size())return false;
  if(!parsed.tile_ranges.empty() && parsed.tile_ranges.size()!=parsed.tiles.size())return false;
  for(std::size_t index=0;index<parsed.tile_ranges.size();++index) {
    const auto& range=parsed.tile_ranges[index];
    if(!range[1]) {if(range[0])return false;continue;}
    if(range[1]<20 || range[1]>64ull*1024*1024 || range[0]>UINT64_MAX-range[1])return false;
  }
  std::set<std::string> ids;
  for(const auto& id:parsed.tile_ids) {
    if(id.empty() || id.size()>128 || !ids.insert(id).second ||
        std::any_of(id.begin(),id.end(),[](unsigned char c){return c<33 || c>126;}))return false;
  }
  directory_ = manifest_path.parent_path();source_=directory_/std::filesystem::u8path(parsed.map);
  texture_cache_=manifest_path;texture_cache_+=".textures";
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
    tile.file = parsed.map;tile.id="tile/0";
    const auto relative=std::filesystem::u8path(tile.file);
    if(relative.is_absolute() || relative.has_root_name() ||
        std::any_of(relative.begin(),relative.end(),[](const auto& part){return part=="..";}))return false;
    tile.bounds[0] = tile.bounds[1] = tile.bounds[2] = -1e9f;
    tile.bounds[3] = tile.bounds[4] = tile.bounds[5] = 1e9f;
  } else {
    for (std::size_t index = 0; index < parsed.tiles.size(); ++index) {
      WorldTile& tile = tiles_.emplace_back();
      tile.file = parsed.tile_files[index];tile.id=parsed.tile_ids.empty()?"tile/"+std::to_string(index):parsed.tile_ids[index];
      tile.collision=parsed.tile_collision.empty() || parsed.tile_collision[index];
      if(!parsed.tile_ranges.empty()) {tile.source_offset=parsed.tile_ranges[index][0];tile.source_length=parsed.tile_ranges[index][1];}
      if(tile.id.empty() || tile.id.size()>128 || (parsed.tile_ids.empty() && !ids.insert(tile.id).second))return false;
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
      if(tile.source_length) {
        std::error_code range_error;
        const auto bytes=std::filesystem::file_size(directory_/relative,range_error);
        if(range_error || tile.source_offset>bytes || tile.source_length>bytes-tile.source_offset)return false;
      }
    }
  }
  if(parsed.tile_residency!="" && parsed.tile_residency!="distance" && parsed.tile_residency!="external")return false;
  external_=parsed.tile_residency=="external";initial_wanted_.clear();
  if(external_) {
    if(parsed.tile_initial_wanted.empty() || parsed.tile_initial_wanted.size()>tiles_.size())return false;
    std::set<std::uint32_t> unique;
    for(auto index:parsed.tile_initial_wanted)if(index>=tiles_.size() || !unique.insert(index).second)return false;
    initial_wanted_=std::move(parsed.tile_initial_wanted);
  } else if(!parsed.tile_initial_wanted.empty())return false;
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
