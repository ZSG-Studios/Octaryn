#pragma once
#include <filesystem>

namespace octaryn::client::app {

// Blender-exported GLB map description shipped with the client bundle.
struct MapManifest {
  std::filesystem::path glb; // Absolute path to the map payload.
  std::filesystem::path manifest;
  bool tiled{};
  float spawn_x{}, spawn_y{}, spawn_z{}; // Eye position in map space (+Y up).
  float yaw{}, pitch{};                  // Initial view angles, radians.
};

// True when the bundle ships Assets/Maps/map.json (or OCTARYN_CLIENT_MAP_MANIFEST
// points at one); the engine renders the mesh map world at startup.
bool map_mode_available(const std::filesystem::path& bundle);

// Parses the bundled manifest; false (with a stderr diagnostic) on any
// malformed manifest so startup refuses an undefined world.
bool load_map_manifest(const std::filesystem::path& bundle, MapManifest& out);
// Same contract for an explicit manifest path (world-selector entries).
bool load_map_manifest_from(const std::filesystem::path& manifest_path, MapManifest& out);

}
