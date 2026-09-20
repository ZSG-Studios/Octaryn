#include "MapWorld.h"

#include "MapSceneGeometry.h"
#include "MapWorldSession.h"

#include <cstdio>
#include <cmath>
#include <filesystem>
#include <limits>
#include <new>
#include <string>

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
bool spawn_floor_ray(const MapTriangleSoup &soup, float x, float eye_y,
                     float z, float search, float &floor_y) {
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

  if (!octaryn::server::map_world::load_map_triangle_soup(glb_path,
                                                          world->soup) ||
      !octaryn::server::map_world::parse_map_manifest(manifest_path,
                                                      world->manifest)) {
    delete world;
    return nullptr;
  }

  // Snap the eye spawn onto the highest floor triangle below it. Blender
  // exports disagree about the eye offset and winding; landing the feet on the
  // real floor prevents the fall-forever startup. Winding-agnostic ray.
  constexpr float kEyeOffset = 1.62f;
  constexpr float kFloorSearchBlocks = 4096.0f;
  float floor_y = 0.0f;
  const bool floor_found = octaryn::server::map_world::spawn_floor_ray(
      world->soup, world->manifest.spawn_x, world->manifest.spawn_y + 1.0f,
      world->manifest.spawn_z, kFloorSearchBlocks, floor_y);
  if (floor_found) {
    world->manifest.spawn_y = floor_y + kEyeOffset;
  }

  std::fprintf(stderr,
               "server_live_map_world_load vertices=%zu triangles=%zu "
               "spawn=(%.3f,%.3f,%.3f) yaw=%.6f pitch=%.6f floor=%s "
               "floor_y=%.3f\n",
               world->soup.positions.size() / 3u, world->soup.triangle_count(),
               world->manifest.spawn_x, world->manifest.spawn_y,
               world->manifest.spawn_z, world->manifest.yaw,
               world->manifest.pitch, floor_found ? "hit" : "none", floor_y);
  return world;
}

void octaryn_server_map_world_destroy(void *handle) {
  delete static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
}

unsigned long long octaryn_server_map_world_triangle_count(void *handle) {
  const auto *world =
      static_cast<const octaryn::server::map_world::ServerMapWorld *>(handle);
  return world != nullptr
             ? static_cast<unsigned long long>(world->soup.triangle_count())
             : 0ull;
}

} // extern "C"
