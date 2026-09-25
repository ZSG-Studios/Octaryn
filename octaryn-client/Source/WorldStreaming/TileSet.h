#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::app {

// One streamed world tile: a GLB payload plus its world-space bounds.
struct WorldTile {
  std::string file;      // Relative to the manifest directory.
  float bounds[6]{};     // min x/y/z, max x/y/z.
  bool resident{};       // Set by the streaming session as tiles load/evict.
};

struct TileResidency {
  std::uint32_t wanted{};        // Tiles inside the streaming radius.
  std::uint32_t resident{};      // Currently loaded tiles.
  std::uint32_t load_queue{};    // Wanted but not yet resident.
  std::uint32_t evict_queue{};   // Resident but outside the keep radius.
  float nearest_distance{-1.0f}; // Distance to the nearest wanted tile.
};

// First-party world streaming: tile-set manifest plus camera-driven
// residency. The session drives loads/evictions from these decisions; the
// renderer consumes per-tile MapRenderers.
class TileSet {
public:
  // Reads a manifest; a manifest without a "tiles" array keeps the single
  // implicit "map" payload as a one-tile set.
  bool load(const std::filesystem::path& manifest_path);
  const WorldTile* tile(std::uint32_t index) const;
  std::uint32_t tile_count() const { return static_cast<std::uint32_t>(tiles_.size()); }
  const std::filesystem::path& payload_directory() const { return directory_; }

  // Residency decision for a camera position: wanted tiles inside the radius,
  // ordered nearest first, with load/evict queues against current residency.
  TileResidency evaluate(float camera_x, float camera_y, float camera_z,
                         float load_radius, float keep_radius) const;
  WorldTile* mutable_tile(std::uint32_t index);

private:
  std::vector<WorldTile> tiles_;
  std::filesystem::path directory_;
};

}
