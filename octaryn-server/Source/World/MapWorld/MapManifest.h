#pragma once

#include <filesystem>

namespace octaryn::server::map_world {

// Parsed map.json: spawn is the EYE position in glTF world space (+Y up),
// yaw/pitch are radians.
struct MapManifest {
  int version = 1;
  float spawn_x = 0.0f;
  float spawn_y = 0.0f;
  float spawn_z = 0.0f;
  float yaw = 0.0f;
  float pitch = -0.35f;
};

// Parses a version-1 map manifest; false on unreadable or unsupported files.
bool parse_map_manifest(const std::filesystem::path &manifest_path,
                        MapManifest &manifest);

} // namespace octaryn::server::map_world
