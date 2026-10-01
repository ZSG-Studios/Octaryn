#pragma once

#include "MapManifest.h"
#include "MapSceneGeometry.h"
#include "CollisionResidency.h"

namespace octaryn::server::map_world {

// Loaded map world: one static collision soup plus the manifest spawn pose.
struct ServerMapWorld {
  MapTriangleSoup soup;
  MapManifest manifest;
  size_t triangle_count{};
  std::unique_ptr<CollisionResidency> tiles;
  bool ready(float x, float y, float z, float radius) { return !tiles || tiles->ready(x, y, z, radius); }
  bool ready_bounds(const std::array<float,6>& bounds) {return !tiles || tiles->ready_bounds(bounds);}
  character_motion::MeshCollision collision() {
    if (tiles) return tiles->collision();
    return {soup.positions.data(), soup.positions.size(), soup.indices.data(), soup.indices.size()};
  }
};

} // namespace octaryn::server::map_world
