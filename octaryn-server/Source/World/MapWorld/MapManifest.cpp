#include "MapManifest.h"

#include <glaze/glaze.hpp>

#include <array>
#include <cmath>
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
};

} // namespace octaryn::server::map_world

namespace {

constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};

using octaryn::server::map_world::map_manifest_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::error_code error;
  const auto size=std::filesystem::file_size(path,error);
  if(error || size==0 || size>1024u*1024u)return false;
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool is_supported(const map_manifest_file &file) {
  if(file.tile_files.size()!=file.tiles.size() || file.tile_files.size()>65536)return false;
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
  if (!is_supported(file)) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=manifest_version\n");
    return false;
  }

  manifest.version = file.version;
  manifest.spawn_x = file.spawn[0];
  manifest.spawn_y = file.spawn[1];
  manifest.spawn_z = file.spawn[2];
  manifest.yaw = file.yaw;
  manifest.pitch = file.pitch;
  manifest.tile_files = std::move(file.tile_files);
  manifest.tiles = std::move(file.tiles);
  return true;
}

} // namespace octaryn::server::map_world
