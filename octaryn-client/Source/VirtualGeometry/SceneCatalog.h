#pragma once
#include "GeometryFormat.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::rendering::virtual_geometry {
inline constexpr std::uint32_t scene_catalog_version=4;
struct SceneResource {
  std::string path,hash;
  std::uint64_t bytes{};
};
struct ScenePrimitive {
  std::uint32_t mesh{},primitive{},material{invalid_id},first_part{},part_count{};
  std::uint64_t triangles{},vertices{};
  std::array<float,6> bounds{};
  bool position_only{};
  // Empty means source order. Otherwise part ranges index this source-bound permutation.
  std::string triangle_order,triangle_order_hash;
  MapMaterial surface;
};
struct ScenePart {
  std::uint32_t primitive{};
  std::uint64_t first_triangle{},triangle_count{};
  std::array<float,6> bounds{};
  std::string geometry,hash;
  std::uint32_t clusters{},pages{},root_pages{};
  bool bounds_prepared{};
};
struct SceneInstance {
  std::uint32_t node{},mesh{};
  std::string name;
  std::array<float,16> transform{};
  std::array<float,6> bounds{};
};
struct SceneCatalog {
  std::uint32_t version{scene_catalog_version},part_triangles{65536},mesh_count{},material_count{};
  std::uint64_t unique_triangles{},instanced_triangles{};
  std::string source,source_hash;
  std::vector<SceneResource> resources;
  std::vector<ScenePrimitive> primitives;
  std::vector<ScenePart> parts;
  std::vector<SceneInstance> instances;
};
bool import_scene_catalog(const std::filesystem::path&,SceneCatalog&,std::string&,const std::atomic_bool* cancel=nullptr);
bool validate_scene_catalog(const SceneCatalog&,std::string&);
bool read_scene_catalog(const std::filesystem::path&,SceneCatalog&,std::string&);
bool write_scene_catalog(const std::filesystem::path&,const SceneCatalog&,std::string&);
}
