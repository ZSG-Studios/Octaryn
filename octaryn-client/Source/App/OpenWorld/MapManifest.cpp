#include "MapManifest.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
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
  std::optional<std::string> texture_cache;
};

namespace {

bool read_text(const std::filesystem::path& path, std::string& text) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return false;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > 1024u * 1024u) return false;
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
  out.manifest = path;
  out.tiled = parsed.tiles && !parsed.tiles->empty();
  out.glb = path.parent_path() / std::filesystem::u8path(parsed.map);
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
