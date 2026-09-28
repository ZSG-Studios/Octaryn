#pragma once
#include "MapManifest.h"
#include "MeshCollisionScene.h"
#include "CollisionResidencyStats.h"
#include <filesystem>
#include <memory>

namespace octaryn::server::map_world {
using CollisionResidencyStats = OctarynCollisionResidencyStats;

// All calls except preparation run between queries on the authority owner.
class CollisionResidency {
public:
  CollisionResidency(const MapManifest& manifest, const std::filesystem::path& directory);
  ~CollisionResidency();
  CollisionResidency(const CollisionResidency&) = delete;
  CollisionResidency& operator=(const CollisionResidency&) = delete;
  bool ready(float x, float z, float radius);
  character_motion::MeshCollision collision();
  CollisionResidencyStats stats() const;
  uint64_t triangle_count() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
