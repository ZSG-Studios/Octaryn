#pragma once

#include "CharacterMotion.h"

#include <box3d/box3d.h>

namespace octaryn::character_motion {

// Box3D world holding one static triangle-soup body. Only in-library headers
// include this; the public CharacterMotion.h stays backend-free.
struct MeshCollisionWorld {
  b3WorldId world{};
  b3BodyId body{};
  b3MeshData *mesh{};
  ~MeshCollisionWorld();
  MeshCollisionWorld() = default;
  MeshCollisionWorld(const MeshCollisionWorld &) = delete;
  MeshCollisionWorld &operator=(const MeshCollisionWorld &) = delete;
};

// Returns the cached world for the soup, building it on first use. The soup
// must outlive every step against it. Null when the soup cannot produce a
// collision mesh.
MeshCollisionWorld *acquire_mesh_world(const MeshCollision &mesh);
b3MeshData* build_collision_mesh(const MeshCollision& mesh, bool* valid_empty = nullptr);

} // namespace octaryn::character_motion
