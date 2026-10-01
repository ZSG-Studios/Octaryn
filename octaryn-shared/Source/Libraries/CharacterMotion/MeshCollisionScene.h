#pragma once
#include "CharacterMotion.h"
#include <memory>

namespace octaryn::character_motion {
struct MeshCollisionWorld;
class PreparedCollisionTile {
public:
  ~PreparedCollisionTile();
  PreparedCollisionTile(const PreparedCollisionTile&) = delete;
  PreparedCollisionTile& operator=(const PreparedCollisionTile&) = delete;
private:
  friend class MeshCollisionScene;
  explicit PreparedCollisionTile(void* mesh) : mesh_(mesh) {}
  void* mesh_;
};

// Owner-thread tile membership. Geometry is copied into independent Box3D
// meshes; adding/removing a tile never rebuilds another tile's collision mesh.
class MeshCollisionScene {
public:
  MeshCollisionScene();
  ~MeshCollisionScene();
  MeshCollisionScene(const MeshCollisionScene&) = delete;
  MeshCollisionScene& operator=(const MeshCollisionScene&) = delete;
  bool set_tile(uint64_t id, const MeshCollision& mesh);
  // A non-null handle may be valid empty collision after Box3D's area rejection.
  // BVH construction is world-independent and may run on an asset worker.
  static std::unique_ptr<PreparedCollisionTile> prepare_tile(const MeshCollision& mesh);
  // Publication and removal stay on the world owner thread.
  bool set_tile(uint64_t id, std::unique_ptr<PreparedCollisionTile> prepared);
  void remove_tile(uint64_t id);
  bool contains(uint64_t id) const;
  MeshCollision view();
  MeshCollisionWorld* collision_world();
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
