#include "MapManifest.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
  return bundle / "Client" / "Assets" / "Maps" / "map.json";
}

} // namespace

bool map_mode_available(const std::filesystem::path& bundle) {
  return std::filesystem::is_regular_file(manifest_path(bundle));
}

bool load_map_manifest(const std::filesystem::path& bundle, MapManifest& out) {
  return load_map_manifest_from(bundle / "Client" / "Assets" / "Maps" / "map.json", out);
}

bool load_map_manifest_from(const std::filesystem::path& manifest_path, MapManifest& out) {
  const auto path = manifest_path;
  std::string text;
  if (!read_text(path, text)) {
    std::fprintf(stderr, "Map manifest unreadable: %s\n", path.generic_string().c_str());
    return false;
  }
  MapManifestFile parsed;
  constexpr glz::opts options{.error_on_unknown_keys = true, .error_on_missing_keys = true};
  if (glz::read<options>(parsed, text) || parsed.version != 1 || parsed.map.empty() ||
      parsed.map.find('/') != std::string::npos || parsed.map.find('\\') != std::string::npos) {
    std::fprintf(stderr, "Map manifest invalid: %s\n", path.generic_string().c_str());
    return false;
  }
  for (const float value : parsed.spawn) {
    if (!std::isfinite(value)) {
      std::fprintf(stderr, "Map manifest spawn is not finite\n");
      return false;
    }
  }
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
