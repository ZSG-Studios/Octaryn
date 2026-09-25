#pragma once
#include <filesystem>

namespace octaryn::client::app {

// Blender-exported GLB map description shipped with the client bundle.
struct MapManifest {
  std::filesystem::path glb; // Absolute path to the map payload.
  float spawn_x{}, spawn_y{}, spawn_z{}; // Eye position in map space (+Y up).
  float yaw{}, pitch{};                  // Initial view angles, radians.
};

// True when the bundle ships Assets/Maps/map.json; the bundle then plays the
// mesh map world instead of generated voxel terrain. OCTARYN_CLIENT_MAP_MODE=0
// forces the voxel path for qualification tooling.
bool map_mode_available(const std::filesystem::path& bundle);

// Parses the bundled manifest; false (with a stderr diagnostic) on any
// malformed manifest so startup refuses an undefined world.
bool load_map_manifest(const std::filesystem::path& bundle, MapManifest& out);

} // namespace octaryn::client::app
