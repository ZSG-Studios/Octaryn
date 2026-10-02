#include "MapManifest.h"
#include "WorldLibrary.h"
#include "TileCatalogFiles.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <set>
#include <string_view>
#include <vector>
#include <glaze/glaze.hpp>

namespace octaryn::client::app {

// Named at namespace scope: clang requires external linkage for the glaze
// reflection variable instantiated from an anonymous-namespace struct.
struct MapManifestFile {
  int version{};
  std::string map;
  std::array<float, 3> spawn{};
  float yaw{};
  float pitch{};
  std::optional<std::vector<std::array<float, 6>>> tiles;
  std::optional<std::vector<std::string>> tile_files;
  std::optional<std::vector<std::string>> tile_ids;
  std::optional<std::vector<bool>> tile_collision;
  std::optional<std::vector<std::array<std::uint64_t,2>>> tile_ranges;
  std::optional<std::uint32_t> tile_gpu_budget_mib;
  std::optional<std::vector<std::string>> tile_catalogs;
  std::optional<std::string> texture_cache;
  std::optional<std::string> tile_residency;
  std::optional<std::vector<std::uint32_t>> tile_initial_wanted;
  std::optional<std::string> scene_catalog;
  std::optional<std::string> scene_hierarchy;
  std::optional<std::string> scene_asset;
  std::optional<std::string> scene_descriptor;
};

namespace {

bool read_text(const std::filesystem::path& path, std::string& text) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return false;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > 4u * 1024u * 1024u) return false;
  text.resize(static_cast<std::size_t>(size));
  stream.read(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<std::size_t>(stream.gcount()) == text.size();
}

std::filesystem::path manifest_path(const std::filesystem::path& bundle) {
  if (const char* override_path = std::getenv("OCTARYN_CLIENT_MAP_MANIFEST")) {
    if (*override_path) return std::filesystem::path(reinterpret_cast<const char8_t*>(override_path));
  }
  if(const char* profile=std::getenv("OCTARYN_CLIENT_PERFORMANCE_PROFILE"))
    if(std::string_view(profile)=="HQ200")return bundle / "Client" / "Assets" / "Maps" / "hq200.json";
  return bundle / "Client" / "Assets" / "Maps" / "map.json";
}

} // namespace

bool map_mode_available(const std::filesystem::path& bundle) {
  return std::filesystem::is_regular_file(manifest_path(bundle));
}

bool load_map_manifest(const std::filesystem::path& bundle, MapManifest& out) {
  return load_map_manifest_from(manifest_path(bundle), out);
}

bool load_world_manifest(const std::filesystem::path& world,const std::filesystem::path& bundle,MapManifest& out) {
  if(std::filesystem::exists(world/"world.json")) {
    std::string error;
    const bool loaded=WorldLibrary::resolve(world,out,error);
    if(!loaded)std::fprintf(stderr,"World save cannot resolve its map: %s\n",error.c_str());
    return loaded;
  }
  return std::filesystem::exists(world/"map.json")?load_map_manifest_from(world/"map.json",out):load_map_manifest(bundle,out);
}

bool load_map_manifest_from(const std::filesystem::path& manifest_path, MapManifest& out) {
  const auto path = std::filesystem::absolute(manifest_path);
  std::string text;
  if (!read_text(path, text)) {
    std::fprintf(stderr, "Map manifest unreadable: %s\n", path.generic_string().c_str());
    return false;
  }
  MapManifestFile parsed;
  constexpr glz::opts options{.error_on_unknown_keys = true, .error_on_missing_keys = true};
  if (glz::read<options>(parsed, text) || parsed.version != 1 || parsed.map.empty()) {
    std::fprintf(stderr, "Map manifest invalid: %s\n", path.generic_string().c_str());
    return false;
  }
  if(parsed.tile_catalogs) {
    if(parsed.tile_catalogs->empty())return false;
    content::TileCatalogArrays arrays{parsed.version,parsed.tiles.value_or(std::vector<std::array<float,6>>{}),
        parsed.tile_files.value_or(std::vector<std::string>{}),parsed.tile_ids.value_or(std::vector<std::string>{}),
        parsed.tile_collision.value_or(std::vector<bool>{}),parsed.tile_ranges.value_or(std::vector<std::array<std::uint64_t,2>>{})};
    if(!content::expand_tile_catalogs(path,*parsed.tile_catalogs,arrays))return false;
    parsed.tiles=std::move(arrays.tiles);parsed.tile_files=std::move(arrays.tile_files);
    parsed.tile_ids=std::move(arrays.tile_ids);parsed.tile_collision=std::move(arrays.tile_collision);
    parsed.tile_ranges=std::move(arrays.tile_ranges);
  }
  if(parsed.tile_gpu_budget_mib && (*parsed.tile_gpu_budget_mib<64 || *parsed.tile_gpu_budget_mib>32768))return false;
  for (const float value : parsed.spawn) {
    if (!std::isfinite(value)) {
      std::fprintf(stderr, "Map manifest spawn is not finite\n");
      return false;
    }
  }
  if (parsed.tiles.has_value() != parsed.tile_files.has_value() ||
      (parsed.tiles && (parsed.tiles->size() != parsed.tile_files->size() || parsed.tiles->size() > 65536))) {
    std::fprintf(stderr, "Map manifest tile arrays are inconsistent\n");
    return false;
  }
  if(parsed.tile_ids) {
    if(!parsed.tiles || parsed.tile_ids->size()!=parsed.tiles->size())return false;
    std::set<std::string> ids;
    for(const auto& id:*parsed.tile_ids)if(id.empty() || id.size()>128 || !ids.insert(id).second ||
        std::any_of(id.begin(),id.end(),[](unsigned char c){return c<33 || c>126;}))return false;
  }
  if(parsed.tile_collision && (!parsed.tiles || parsed.tile_collision->size()!=parsed.tiles->size()))return false;
  if(parsed.tile_ranges) {
    if(!parsed.tiles || parsed.tile_ranges->size()!=parsed.tiles->size())return false;
    for(std::size_t index=0;index<parsed.tile_ranges->size();++index) {
      const auto& range=(*parsed.tile_ranges)[index];
      if(!range[1]) {if(range[0])return false;continue;}
      if(range[1]<20 || range[1]>64ull*1024*1024 || range[0]>UINT64_MAX-range[1])return false;
      std::error_code range_error;
      const auto size=std::filesystem::file_size(path.parent_path()/std::filesystem::u8path((*parsed.tile_files)[index]),range_error);
      if(range_error || range[0]>size || range[1]>size-range[0])return false;
    }
  }
  if(parsed.tile_residency && *parsed.tile_residency!="distance" && *parsed.tile_residency!="external")return false;
  if(parsed.tile_residency && *parsed.tile_residency=="external") {
    if(!parsed.tiles || parsed.tiles->empty() || !parsed.tile_initial_wanted || parsed.tile_initial_wanted->empty())return false;
    std::set<std::uint32_t> wanted;
    for(auto index:*parsed.tile_initial_wanted)if(index>=parsed.tiles->size() || !wanted.insert(index).second)return false;
  } else if(parsed.tile_initial_wanted)return false;
  const auto map_file=std::filesystem::path(reinterpret_cast<const char8_t*>(parsed.map.c_str()));
  if(map_file.is_absolute() || map_file.has_root_name())return false;
  for(const auto& part:map_file)if(part=="..")return false;
  if(parsed.texture_cache) {
    const auto cache=std::filesystem::u8path(*parsed.texture_cache);
    if(cache.empty() || cache.is_absolute() || cache.has_root_name())return false;
    for(const auto& part:cache)if(part=="..")return false;
  }
  if (parsed.tiles) for (std::size_t index = 0; index < parsed.tiles->size(); ++index) {
    const auto& bounds = (*parsed.tiles)[index];
    const auto file = std::filesystem::u8path((*parsed.tile_files)[index]);
    bool valid = !file.empty() && !file.is_absolute() && !file.has_root_name();
    for (const auto& part : file) valid = valid && part != "..";
    for (float value : bounds) valid = valid && std::isfinite(value);
    for (unsigned axis = 0; axis < 3; ++axis) valid = valid && bounds[axis] <= bounds[axis + 3];
    if (!valid || !std::filesystem::is_regular_file(path.parent_path() / file)) {
      std::fprintf(stderr, "Map manifest tile invalid: %zu\n", index);
      return false;
    }
  }
  out.manifest = path;out.authority_spawn_manifest.clear();
  out.tiled = parsed.tiles && !parsed.tiles->empty();
  out.scene_descriptor.clear();
  if(parsed.scene_descriptor) {
    const auto descriptor=std::filesystem::path(reinterpret_cast<const char8_t*>(parsed.scene_descriptor->c_str()));
    if(descriptor.empty() || descriptor.is_absolute() || descriptor.has_root_name())return false;
    for(const auto& part:descriptor)if(part==".." || part==".")return false;
    out.scene_descriptor=path.parent_path()/descriptor;
    if(!std::filesystem::is_regular_file(out.scene_descriptor))return false;
  }
  out.scene_catalog.clear();
  out.scene_hierarchy.clear();
  if(parsed.scene_catalog) {
    const auto catalog=std::filesystem::u8path(*parsed.scene_catalog);
    if(out.tiled || catalog.empty())return false;
    if(!catalog.is_absolute()) {
      if(catalog.has_root_name())return false;
      for(const auto& part:catalog)if(part=="..")return false;
    }
    out.scene_catalog=catalog.is_absolute()?catalog:path.parent_path()/catalog;
    if(!std::filesystem::is_regular_file(out.scene_catalog))return false;
  }
  if(parsed.scene_hierarchy) {
    if(out.scene_catalog.empty() || parsed.scene_hierarchy->empty())return false;
    const auto hierarchy=std::filesystem::u8path(*parsed.scene_hierarchy);
    if(!hierarchy.is_absolute()) {
      if(hierarchy.has_root_name())return false;
      for(const auto& part:hierarchy)if(part=="..")return false;
    }
    out.scene_hierarchy=hierarchy.is_absolute()?hierarchy:path.parent_path()/hierarchy;
    std::error_code ec;
    if(!std::filesystem::equivalent(out.scene_hierarchy,out.scene_catalog.parent_path()/"hierarchy"/"scene.json",ec) || ec)return false;
  }
  out.glb = path.parent_path() / std::filesystem::u8path(parsed.map);
  out.scene_asset=parsed.scene_asset.value_or("");
  if(out.scene_asset.size()>255 || out.scene_asset.find_first_of("\r\n\t")!=std::string::npos)return false;
  if (!std::filesystem::is_regular_file(out.glb)) {
    std::fprintf(stderr, "Map payload missing: %s\n", out.glb.generic_string().c_str());
    return false;
  }
  out.spawn_x = parsed.spawn[0];
  out.spawn_y = parsed.spawn[1];
  out.spawn_z = parsed.spawn[2];
  out.yaw = std::isfinite(parsed.yaw) ? parsed.yaw : 0.0f;
  out.pitch = std::isfinite(parsed.pitch) ? parsed.pitch : 0.0f;
  return true;
}

} // namespace octaryn::client::app
