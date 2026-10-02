#include "MapWorld.h"

#include "CharacterMotion.h"
#include "MapSceneGeometry.h"
#include "SceneCollisionBodies.h"
#include "MapWorldSession.h"
#include "MapTransferSpawn.h"
#include "MeshCollisionWorld.h"

#include <box3d/box3d.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <limits>
#include <new>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

namespace octaryn::server::map_world {
namespace {

// The server hands out absolute UTF-8 paths; narrow ANSI construction would
// mangle non-ASCII paths on Windows.
std::filesystem::path utf8_path(const char *path_utf8) {
  return std::filesystem::path{
      std::u8string{reinterpret_cast<const char8_t *>(path_utf8)}};
}

// Highest triangle hit on a straight down-ray below the eye spawn. Blender
// exports disagree about winding, so this test is winding-agnostic. Returns
// false when no triangle exists under the spawn within the search distance.
bool spawn_floor_ray(ServerMapWorld &world, float x, float eye_y,
                     float z, float search, float &floor_y,std::chrono::steady_clock::time_point deadline) {
  if (world.tiles) {
    while(!world.ready_bounds({x-.01f,eye_y-search-.01f,z-.01f,x+.01f,eye_y+.01f,z+.01f})) {
      if(world.tiles->stats().failed || std::chrono::steady_clock::now()>deadline)return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto* collision = character_motion::acquire_mesh_world(world.collision());
    if (!collision) return false;
    const auto down = b3World_CastRayClosest(collision->world, {x, eye_y, z}, {0, -search, 0}, b3DefaultQueryFilter());
    bool hit = down.hit;
    floor_y = hit ? static_cast<float>(down.point.y) : -std::numeric_limits<float>::infinity();
    // Mesh rays are one-sided. Ascending backface hits preserve the old
    // winding-independent spawn search without duplicating collision meshes.
    float bottom = eye_y - search;
    for (unsigned layer = 0; layer < 256; ++layer) {
      const auto up = b3World_CastRayClosest(collision->world, {x, bottom, z}, {0, eye_y - bottom, 0}, b3DefaultQueryFilter());
      if (!up.hit) return hit;
      hit = true;
      floor_y = std::max(floor_y, static_cast<float>(up.point.y));
      bottom = static_cast<float>(up.point.y) + .0001f;
      if (bottom >= eye_y) return hit;
    }
    std::fprintf(stderr, "server_spawn_floor failed reason=backface_layer_limit\n");
    return false;
  }
  const auto& soup = world.soup;
  bool found = false;
  float best = -std::numeric_limits<float>::infinity();
  const std::size_t triangles = soup.indices.size() / 3u;
  const float *p = soup.positions.data();
  const uint32_t *idx = soup.indices.data();
  for (std::size_t t = 0; t < triangles; ++t) {
    const uint32_t a = idx[t * 3u], b = idx[t * 3u + 1u],
                   c = idx[t * 3u + 2u];
    if (a * 3u + 2u >= soup.positions.size() ||
        b * 3u + 2u >= soup.positions.size() ||
        c * 3u + 2u >= soup.positions.size()) {
      continue;
    }
    const float *pa = p + a * 3u, *pb = p + b * 3u, *pc = p + c * 3u;
    // Degenerate triangles cannot support the player.
    const float e1[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
    const float e2[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                        e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] < 1e-12f) {
      continue;
    }
    // Ray (x, eye_y, z) + s * (0, -1, 0): solve barycentric plane intersection.
    const float d = n[0] * (x - pa[0]) + n[1] * (eye_y - pa[1]) +
                    n[2] * (z - pa[2]);
    if (std::abs(n[1]) < 1e-9f) {
      continue; // Vertical plane cannot be the floor directly below.
    }
    const float s = d / n[1]; // distance above the plane along -Y
    if (s < 0.0f || s > search) {
      continue; // Plane crosses below the feet or beyond the search distance.
    }
    const float y = eye_y - s;
    // Point-in-triangle on the XZ plane.
    const float d00 = e1[0] * e1[0] + e1[2] * e1[2];
    const float d01 = e1[0] * e2[0] + e1[2] * e2[2];
    const float d11 = e2[0] * e2[0] + e2[2] * e2[2];
    const float vx[2] = {x - pa[0], z - pa[2]};
    const float v20[2] = {vx[0], vx[1]};
    const float d20 = v20[0] * e1[0] + v20[1] * e1[2];
    const float d21 = v20[0] * e2[0] + v20[1] * e2[2];
    const float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-12f) {
      continue;
    }
    const float u = (d11 * d20 - d01 * d21) / denom;
    const float v = (d00 * d21 - d01 * d20) / denom;
    if (u < -1e-4f || v < -1e-4f || u + v > 1.0f + 1e-4f) {
      continue;
    }
    if (y > best) {
      best = y;
      found = true;
    }
  }
  if (found) {
    floor_y = best;
  }
  return found;
}

// Real map geometry has holes at plaza edges, courtyards and doorsteps, so a
// manifest spawn can land over a gap. Ring-search outward for the closest
// solid floor instead of spawning into the void.
bool spawn_floor_search(ServerMapWorld &world, float x, float eye_y,
                        float z, float search, float &floor_x,
                        float &floor_y, float &floor_z,std::chrono::steady_clock::time_point deadline) {
  if (spawn_floor_ray(world, x, eye_y, z, search, floor_y,deadline)) {
    floor_x = x;
    floor_z = z;
    return true;
  }
  constexpr float kStep = 0.5f;
  constexpr int kMaxRing = 16; // 8m radius
  for (int ring = 1; ring <= kMaxRing; ++ring) {
    const float radius = static_cast<float>(ring) * kStep;
    const int steps = 8 * ring;
    for (int s = 0; s < steps; ++s) {
      if(std::chrono::steady_clock::now()>deadline || (world.tiles && world.tiles->stats().failed))return false;
      const float angle = static_cast<float>(s) *
                          (6.28318530718f / static_cast<float>(steps));
      const float cx = x + radius * std::cos(angle);
      const float cz = z + radius * std::sin(angle);
      if (spawn_floor_ray(world, cx, eye_y, cz, search, floor_y,deadline)) {
        floor_x = cx;
        floor_z = cz;
        return true;
      }
    }
  }
  return false;
}

} // namespace
} // namespace octaryn::server::map_world

extern "C" {

void *octaryn_server_map_world_create(const char *glb_path_utf8,
                                      const char *manifest_path_utf8) {
  if (glb_path_utf8 == nullptr || glb_path_utf8[0] == '\0') {
    return nullptr;
  }

  const std::filesystem::path glb_path =
      octaryn::server::map_world::utf8_path(glb_path_utf8);
  const std::filesystem::path manifest_path =
      manifest_path_utf8 != nullptr && manifest_path_utf8[0] != '\0'
          ? octaryn::server::map_world::utf8_path(manifest_path_utf8)
          : glb_path.parent_path() / "map.json";

  auto *world = new (std::nothrow)
      octaryn::server::map_world::ServerMapWorld{};
  if (world == nullptr) {
    return nullptr;
  }

  if (!octaryn::server::map_world::parse_map_manifest(manifest_path,
                                                      world->manifest)) {
    delete world;
    return nullptr;
  }
  if(!octaryn::server::map_world::apply_map_transfer_spawn(world->manifest)) {
    std::fprintf(stderr,"server_live_map_world_load failed reason=transfer_pose_overlay\n");
    delete world;return nullptr;
  }

  std::string exclusion_error;auto physics_path=glb_path;physics_path.replace_extension(".physics.json");
  if(!octaryn::character_motion::read_scene_body_exclusions(physics_path,world->soup.excluded_nodes,exclusion_error)) {
    std::fprintf(stderr,"server_scene_physics_exclusion_failed reason=%s\n",exclusion_error.c_str());delete world;return nullptr;
  }
  if (!world->manifest.tile_files.empty() || !world->manifest.scene_catalog.empty()) {
    try {
      world->tiles = std::make_unique<octaryn::server::map_world::CollisionResidency>(
          world->manifest,manifest_path.parent_path(),glb_path);
    } catch (const std::exception& error) {
      std::fprintf(stderr, "server_collision_residency failed reason=%s\n", error.what());
      delete world;
      return nullptr;
    }
  } else if (!octaryn::server::map_world::load_map_triangle_soup(glb_path, world->soup)) {
    delete world;
    return nullptr;
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (!world->ready(world->manifest.spawn_x, world->manifest.spawn_y, world->manifest.spawn_z, 3)) {
    if (std::chrono::steady_clock::now() >= deadline || world->tiles->stats().failed) {
      std::fprintf(stderr, "server_live_map_world_load failed reason=collision_residency\n");
      delete world;
      return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Snap the eye spawn onto the highest floor triangle below its feet, searching
  // outward when the manifest point sits over a gap. Blender exports
  // disagree about the eye offset and winding; landing the feet on the real
  // floor prevents the fall-forever startup. Winding-agnostic ray. Only
  // floors within a sane band below the spawn count: exported maps can carry
  // stray geometry hundreds of meters down that must not become the floor.
  constexpr float kEyeOffset = 1.62f;
  constexpr float kSpawnStepTolerance = 0.25f;
  constexpr float kFloorSearchDepth = 24.0f;
  float floor_x = world->manifest.spawn_x;
  float floor_y = 0.0f;
  float floor_z = world->manifest.spawn_z;
  const auto* transfer_policy=std::getenv("OCTARYN_SERVER_MAP_TRANSFER_SPAWN");
  const bool exact_transfer=transfer_policy && std::string_view(transfer_policy)=="1";
  const bool floor_found = !exact_transfer && octaryn::server::map_world::spawn_floor_search(
      *world, world->manifest.spawn_x,
      // Searching from above the eye can mistake a shelf or ceiling for the
      // floor and lift the capsule through the authored room. Permit only a
      // bounded step above the requested feet; higher surfaces are not support.
      world->manifest.spawn_y - kEyeOffset + kSpawnStepTolerance, world->manifest.spawn_z,
      kFloorSearchDepth, floor_x, floor_y, floor_z,deadline);
  if (floor_found) {
    world->manifest.spawn_x = floor_x;
    // A manifest spawn above the floor drops onto it under gravity; a spawn
    // at or below the floor snaps the eye onto the surface instead.
    const float floor_eye_y = floor_y + kEyeOffset;
    world->manifest.spawn_y =
        world->manifest.spawn_y > floor_eye_y ? world->manifest.spawn_y
                                              : floor_eye_y;
    world->manifest.spawn_z = floor_z;
  } else if (!exact_transfer) {
    // A map with no walkable floor near the spawn is unusable; fail the map
    // world load instead of spawning the player into the void.
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=no_floor "
                 "spawn=(%.3f,%.3f,%.3f)\n",
                 world->manifest.spawn_x, world->manifest.spawn_y,
                 world->manifest.spawn_z);
    delete world;
    return nullptr;
  }

  world->triangle_count = world->tiles ? world->tiles->triangle_count() : world->soup.triangle_count();
  if (world->tiles) {
    const auto stats = world->tiles->stats();
    std::fprintf(stderr, world->manifest.scene_catalog.empty()?
        "server_collision_tiles ready=%u total=%zu authority_streaming=1 reserved_bytes=%llu resident_bytes=%llu budget_bytes=%llu\n":
        "server_collision_scene resident=%u authored_tiles=%zu authority_streaming=1 reserved_bytes=%llu resident_bytes=%llu budget_bytes=%llu\n",
                 stats.resident, world->manifest.tiles.size(), static_cast<unsigned long long>(stats.reserved_bytes),
                 static_cast<unsigned long long>(stats.resident_bytes), static_cast<unsigned long long>(stats.budget_bytes));
  }
  std::fprintf(stderr,
               "server_live_map_world_load vertices=%zu triangles=%zu "
               "spawn=(%.3f,%.3f,%.3f) yaw=%.6f pitch=%.6f floor=%s "
               "floor_y=%.3f\n",
               world->soup.positions.size() / 3u, world->triangle_count,
               world->manifest.spawn_x, world->manifest.spawn_y,
               world->manifest.spawn_z, world->manifest.yaw,
               world->manifest.pitch, floor_found ? "hit" : "none", floor_y);
  if (world->tiles) {
    std::vector<float>().swap(world->soup.positions);
    std::vector<uint32_t>().swap(world->soup.indices);
  }
  return world;
}

void octaryn_server_map_world_destroy(void *handle) {
  auto *world =
      static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
  if (world == nullptr) {
    return;
  }
  const auto mesh = world->collision();
  world->bodies.reset();
  octaryn::character_motion::release_mesh_collision(mesh);
  if (world->tiles) {
    const auto s = world->tiles->stats();
    std::fprintf(stderr, "server_collision_residency version=%u resident=%u preparing=%u reserved_bytes=%llu loads=%llu evictions=%llu cancelled=%llu waits=%llu failed=%u\n",
      s.version, s.resident, s.preparing, static_cast<unsigned long long>(s.reserved_bytes),
      static_cast<unsigned long long>(s.loads), static_cast<unsigned long long>(s.evictions),
      static_cast<unsigned long long>(s.cancelled), static_cast<unsigned long long>(s.waits), s.failed);
  }
  delete world;
}

unsigned long long octaryn_server_map_world_triangle_count(void *handle) {
  const auto *world =
      static_cast<const octaryn::server::map_world::ServerMapWorld *>(handle);
  return world != nullptr
             ? static_cast<unsigned long long>(world->tiles ? world->tiles->triangle_count() : world->triangle_count)
             : 0ull;
}

int octaryn_server_map_world_raycast(
    void *handle, float origin_x, float origin_y, float origin_z,
    float direction_x, float direction_y, float direction_z,
    float max_distance, octaryn_host_raycast_hit *out_hit) {
  auto *world =
      static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
  if (world == nullptr || out_hit == nullptr || max_distance <= 0.0f) {
    return -1;
  }

  const float length = std::sqrt(direction_x * direction_x +
                                 direction_y * direction_y +
                                 direction_z * direction_z);
  if (length < 1e-9f) {
    return -2;
  }
  const std::array<float,3> origin{origin_x,origin_y,origin_z};
  const std::array<float,3> endpoint{origin_x+direction_x/length*max_distance,
      origin_y+direction_y/length*max_distance,origin_z+direction_z/length*max_distance};
  std::array<float,6> requested;
  for(unsigned axis=0;axis<3;++axis) {requested[axis]=std::min(origin[axis],endpoint[axis])-.01f;requested[axis+3]=std::max(origin[axis],endpoint[axis])+.01f;}
  if (!world->ready_bounds(requested)) return -4;

  const auto mesh = world->collision();
  octaryn::character_motion::MeshCollisionWorld *collision =
      octaryn::character_motion::acquire_mesh_world(mesh);
  if (collision == nullptr) {
    return -3;
  }

  const float scale = max_distance / length;
  const b3RayResult result = b3World_CastRayClosest(
      collision->world, b3Pos{origin_x, origin_y, origin_z},
      b3Vec3{direction_x * scale, direction_y * scale, direction_z * scale},
      b3DefaultQueryFilter());

  *out_hit = octaryn_host_raycast_hit{};
  if (!result.hit) {
    return 1;
  }

  out_hit->hit = 1u;
  out_hit->material_id = static_cast<uint32_t>(result.userMaterialId);
  out_hit->point_x = result.point.x;
  out_hit->point_y = result.point.y;
  out_hit->point_z = result.point.z;
  out_hit->normal_x = result.normal.x;
  out_hit->normal_y = result.normal.y;
  out_hit->normal_z = result.normal.z;
  out_hit->distance = result.fraction * max_distance;
  out_hit->triangle_index = static_cast<uint32_t>(result.triangleIndex);
  return 0;
}

} // extern "C"
