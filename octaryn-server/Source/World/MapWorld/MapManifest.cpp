#include "MapManifest.h"
#include "FilePath.h"
#include "TileCatalogFiles.h"

#include <glaze/glaze.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace octaryn::server::map_world {

struct map_manifest_file {
  int version = 1;
  std::string map{};
  std::array<float, 3> spawn{};
  float yaw = 0.0f;
  float pitch = -0.35f;
  std::vector<std::string> tile_files;
  std::vector<std::array<float,6>> tiles;
  std::vector<std::string> tile_ids,tile_catalogs;
  std::vector<bool> tile_collision;
  std::vector<std::array<std::uint64_t,2>> tile_ranges;
  std::string scene_catalog;
};

} // namespace octaryn::server::map_world

namespace {

constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};

using octaryn::server::map_world::map_manifest_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::error_code error;
  const auto io=octaryn::content::file_io_path(path);const auto size=std::filesystem::file_size(io,error);
  if(error || size==0 || size>4u*1024u*1024u)return false;
  std::ifstream input{io, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool is_supported(const map_manifest_file &file) {
  if(!file.scene_catalog.empty()) {
    const auto path=std::filesystem::u8path(file.scene_catalog);
    if(!file.tile_files.empty())return false;
    if(!path.is_absolute()) {
      if(path.has_root_name())return false;
      for(const auto& component:path)if(component=="..")return false;
    }
  }
  if(file.tile_files.size()!=file.tiles.size() || file.tile_files.size()>65536)return false;
  if(!file.tile_collision.empty() && file.tile_collision.size()!=file.tiles.size())return false;
  if(!file.tile_ranges.empty() && file.tile_ranges.size()!=file.tiles.size())return false;
  for(std::size_t index=0;index<file.tile_ranges.size();++index) {
    const auto& range=file.tile_ranges[index];
    if(!range[1]) {if(range[0])return false;continue;}
    if(range[1]<20 || range[1]>64ull*1024*1024 || range[0]>UINT64_MAX-range[1])return false;
  }
  for(std::size_t index=0;index<file.tile_files.size();++index) {
    const auto path=std::filesystem::u8path(file.tile_files[index]);
    if(path.empty() || path.is_absolute() || path.has_root_name())return false;
    for(const auto& part:path)if(part=="..")return false;
    const auto& bounds=file.tiles[index];
    for(float value:bounds)if(!std::isfinite(value))return false;
    for(unsigned axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])return false;
  }
  return file.version == 1 && std::isfinite(file.spawn[0]) &&
         std::isfinite(file.spawn[1]) && std::isfinite(file.spawn[2]) &&
         std::isfinite(file.yaw) && std::isfinite(file.pitch);
}

} // namespace

namespace octaryn::server::map_world {

bool parse_map_manifest(const std::filesystem::path &manifest_path,
                        MapManifest &manifest) {
  std::string payload;
  if (!read_text_file(manifest_path, payload)) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=manifest_read\n");
    return false;
  }

  map_manifest_file file{};
  if (glz::read<JsonReadOptions>(file, payload)) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=manifest_json\n");
    return false;
  }
  if(!file.tile_catalogs.empty()) {
    content::TileCatalogArrays arrays{file.version,std::move(file.tiles),std::move(file.tile_files),
        std::move(file.tile_ids),std::move(file.tile_collision),std::move(file.tile_ranges)};
    if(!content::expand_tile_catalogs(manifest_path,file.tile_catalogs,arrays))return false;
    file.tiles=std::move(arrays.tiles);file.tile_files=std::move(arrays.tile_files);
    file.tile_ids=std::move(arrays.tile_ids);file.tile_collision=std::move(arrays.tile_collision);
    file.tile_ranges=std::move(arrays.tile_ranges);
  }
  if (!is_supported(file)) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=manifest_version\n");
    return false;
  }

  manifest.version = file.version;
  for(std::size_t index=0;index<file.tile_ranges.size();++index) {
    const auto& range=file.tile_ranges[index];if(!range[1])continue;
    std::error_code range_error;
    const auto size=std::filesystem::file_size(manifest_path.parent_path()/std::filesystem::u8path(file.tile_files[index]),range_error);
    if(range_error || range[0]>size || range[1]>size-range[0])return false;
  }
  manifest.spawn_x = file.spawn[0];
  manifest.spawn_y = file.spawn[1];
  manifest.spawn_z = file.spawn[2];
  manifest.yaw = file.yaw;
  manifest.pitch = file.pitch;
  manifest.tile_files = std::move(file.tile_files);
  manifest.tiles = std::move(file.tiles);
  manifest.tile_collision = std::move(file.tile_collision);
  manifest.tile_ranges = std::move(file.tile_ranges);
  manifest.scene_catalog=file.scene_catalog.empty()?std::filesystem::path{}:
      manifest_path.parent_path()/std::filesystem::u8path(file.scene_catalog);
  return true;
}

} // namespace octaryn::server::map_world
