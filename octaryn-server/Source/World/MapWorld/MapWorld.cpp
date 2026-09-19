#include "MapWorld.h"

#include "MapSceneGeometry.h"
#include "MapWorldSession.h"

#include <cstdio>
#include <filesystem>
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

  std::fprintf(stderr,
               "server_live_map_world_load vertices=%zu triangles=%zu "
               "spawn=(%.3f,%.3f,%.3f) yaw=%.6f pitch=%.6f\n",
               world->soup.positions.size() / 3u, world->soup.triangle_count(),
               world->manifest.spawn_x, world->manifest.spawn_y,
               world->manifest.spawn_z, world->manifest.yaw,
               world->manifest.pitch);
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
