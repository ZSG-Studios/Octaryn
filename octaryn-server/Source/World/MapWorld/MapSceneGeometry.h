#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace octaryn::server::map_world {

struct MapTriangleSoup {
  std::vector<float> positions;
  std::vector<uint32_t> indices;

  std::size_t triangle_count() const { return indices.size() / 3u; }
};

// Loads every default-scene mesh primitive into one world-space triangle soup.
bool load_map_triangle_soup(const std::filesystem::path &glb_path,
                            MapTriangleSoup &soup);

} // namespace octaryn::server::map_world
