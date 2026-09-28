#include "MeshCollisionWorld.h"
#include "MeshCollisionScene.h"

#include <box3d/box3d.h>
#include <box3d/constants.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace octaryn::character_motion {

MeshCollisionWorld::~MeshCollisionWorld() {
  if (b3World_IsValid(world)) {
    b3DestroyWorld(world);
  }
  if (mesh != nullptr) {
    b3DestroyMesh(mesh);
  }
}

namespace {

struct SoupKey {
  const float *positions;
  const uint32_t *indices;
  bool operator==(const SoupKey &) const = default;
};

struct SoupKeyHash {
  std::size_t operator()(const SoupKey &key) const {
    const auto mix = [](const void *pointer) {
      const auto value = reinterpret_cast<std::uintptr_t>(pointer);
      return std::hash<std::uintptr_t>{}(value >> 4u);
    };
    return mix(key.positions) ^ (mix(key.indices) * 0x9e3779b97f4a7c15ull);
  }
};

// The server steps players from one thread, so the cache is not synchronized.
using WorldCache =
    std::unordered_map<SoupKey, std::unique_ptr<MeshCollisionWorld>,
                       SoupKeyHash>;

WorldCache &world_cache() {
  static WorldCache cache;
  return cache;
}

} // namespace

b3MeshData* build_collision_mesh(const MeshCollision &mesh, bool* valid_empty) {
  if (valid_empty) *valid_empty = false;
  if (!mesh.positions || !mesh.indices) return nullptr;
  const std::size_t vertex_count = mesh.position_count / 3u;
  const std::size_t triangle_count = mesh.index_count / 3u;
  if (mesh.position_count % 3u || mesh.index_count % 3u || vertex_count < 3u || triangle_count == 0u ||
      vertex_count > std::numeric_limits<int>::max() || triangle_count > std::numeric_limits<int>::max()/3) {
    return nullptr;
  }

  // Match Box3D's existing area acceptance without changing rendered geometry.
  std::vector<b3Vec3> vertices;
  vertices.reserve(vertex_count);
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    const float *position = mesh.positions + vertex * 3u;
    if (!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]))return nullptr;
    vertices.push_back({position[0], position[1], position[2]});
  }
  std::vector<int32_t> indices;
  indices.reserve(triangle_count * 3u);
  for (std::size_t triangle = 0; triangle < triangle_count; ++triangle) {
    const uint32_t *source = mesh.indices + triangle * 3u;
    if (source[0] >= vertex_count || source[1] >= vertex_count ||
        source[2] >= vertex_count) {
      return nullptr;
    }
    const auto a=vertices[source[0]],b=vertices[source[1]],c=vertices[source[2]];
    const float area=0.5f*b3Length(b3Cross(b3Sub(b,a),b3Sub(c,a)));
    if (!std::isfinite(area))return nullptr;
    if (area < 0.01f*B3_LINEAR_SLOP*B3_LINEAR_SLOP)continue;
    indices.push_back(static_cast<int32_t>(source[0]));
    indices.push_back(static_cast<int32_t>(source[1]));
    indices.push_back(static_cast<int32_t>(source[2]));
  }
  if (indices.size() < 3u) {
    if (valid_empty) *valid_empty = true;
    std::printf("map_collision_empty source_triangles=%zu accepted_triangles=0 box3d_area_threshold=%.12g\n",
        triangle_count,0.01f*B3_LINEAR_SLOP*B3_LINEAR_SLOP);
    return nullptr;
  }

  b3MeshDef mesh_def{};
  mesh_def.vertices = vertices.data();
  mesh_def.indices = indices.data();
  mesh_def.vertexCount = static_cast<int>(vertex_count);
  mesh_def.triangleCount = static_cast<int>(indices.size() / 3u);
  return b3CreateMesh(&mesh_def, nullptr, 0);
}

namespace {
bool build_world(const MeshCollision &mesh, MeshCollisionWorld &entry) {
  const auto started = std::chrono::steady_clock::now();
  entry.mesh = build_collision_mesh(mesh);
  if (!entry.mesh) return false;
  b3WorldDef world_def = b3DefaultWorldDef();
  world_def.workerCount = 1;
  entry.world = b3CreateWorld(&world_def);
  if (!b3World_IsValid(entry.world)) {
    return false;
  }
  b3BodyDef body_def = b3DefaultBodyDef();
  entry.body = b3CreateBody(entry.world, &body_def);
  if (!b3Body_IsValid(entry.body)) {
    return false;
  }
  b3ShapeDef shape_def = b3DefaultShapeDef();
  const b3ShapeId shape =
      b3CreateMeshShape(entry.body, &shape_def, entry.mesh, b3Vec3_one);
  if (!b3Shape_IsValid(shape)) {
    std::fprintf(stderr, "map_mesh_world_build failed=mesh_shape\n");
    return false;
  }

  const double build_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - started)
          .count();
  std::fprintf(stderr,
               "map_mesh_world_build vertices=%zu triangles=%zu ms=%.1f\n",
               mesh.position_count / 3u, mesh.index_count / 3u, build_ms);
  return true;
}

} // namespace

MeshCollisionWorld *acquire_mesh_world(const MeshCollision &mesh) {
  if (mesh.scene) return mesh.scene->collision_world();
  if (mesh.positions == nullptr || mesh.indices == nullptr) {
    return nullptr;
  }
  const SoupKey key{mesh.positions, mesh.indices};
  auto &cache = world_cache();
  const auto [entry, inserted] = cache.try_emplace(key);
  if (inserted) {
    entry->second = std::make_unique<MeshCollisionWorld>();
    if (!build_world(mesh, *entry->second)) {
      cache.erase(entry);
      return nullptr;
    }
  }
  return entry->second.get();
}

void release_mesh_collision(const MeshCollision &mesh) {
  if (mesh.positions == nullptr || mesh.indices == nullptr) {
    return;
  }
  world_cache().erase(SoupKey{mesh.positions, mesh.indices});
}

} // namespace octaryn::character_motion
