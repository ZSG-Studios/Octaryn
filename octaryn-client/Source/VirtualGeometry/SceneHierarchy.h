#pragma once
#include "SceneCatalog.h"
#include <functional>

namespace octaryn::client::rendering::virtual_geometry {
inline constexpr std::uint32_t scene_hierarchy_version=1;
struct SceneHierarchyGeometry {
  std::string file,hash;
  std::uint64_t triangles{},metadata_bytes{},encoded_bytes{};
  std::uint32_t pages{},clusters{};
  std::vector<std::uint32_t> root_page_ids,page_used_bytes;
  float error{};
};
struct SceneHierarchyNode {
  std::uint32_t id{},leaf_part{invalid_id},leaf_count{};
  std::uint64_t source_triangles{},first_triangle{};
  std::array<float,6> bounds{};
  std::string coverage_hash;
  std::vector<std::uint32_t> children;
  SceneHierarchyGeometry coarse;
};
struct SceneHierarchyShard {
  std::uint32_t version{scene_hierarchy_version},primitive{};
  std::string identity,source_hash,order_hash;
  std::vector<SceneHierarchyNode> nodes;
  std::vector<std::uint32_t> roots;
  bool complete{};
};
struct SceneHierarchyPrimitive {
  std::uint32_t index{},mesh{},primitive{},material{invalid_id},part_count{};
  std::uint64_t source_triangles{};
  bool position_only{},complete{};
  std::array<float,6> bounds{};
  MapMaterial surface;
  std::string order_file,order_hash,shard,shard_hash;
  // Only the complete forest roots: opening never reads leaf geometry metadata.
  std::vector<SceneHierarchyNode> roots;
};
struct SceneHierarchy {
  std::uint32_t version{scene_hierarchy_version},target_triangles{2048},fan_in{4},maximum_triangles{262144};
  std::uint64_t unique_triangles{},instanced_triangles{},leaf_parts{};
  std::string source,source_hash,catalog,identity;
  std::vector<SceneResource> resources;
  std::vector<SceneHierarchyPrimitive> primitives;
  std::vector<SceneInstance> instances;
  bool complete{};
};
struct SceneHierarchyProgress {
  std::uint64_t completed_primitives{},total_primitives{},completed_leaves{},total_leaves{};
  std::uint64_t root_pages{},root_triangles{},root_metadata_bytes{},root_encoded_bytes{},root_payload_bytes{},forest_roots{};
  bool complete{},canceled{};
};
using SceneHierarchyNotify=std::function<void(const SceneHierarchyProgress&)>;
struct SceneHierarchyRequest {
  std::filesystem::path catalog,output;
  std::uint32_t target_triangles{2048},fan_in{4},maximum_triangles{262144};
  std::uint64_t first_primitive{},primitive_count{UINT64_MAX};
  std::uint64_t maximum_new_leaves{UINT64_MAX};
};
bool read_scene_hierarchy(const std::filesystem::path&,SceneHierarchy&,std::string&);
bool read_scene_hierarchy_shard(const std::filesystem::path& package,const SceneHierarchy&,
    std::uint32_t primitive,SceneHierarchyShard&,std::string&);
bool validate_scene_hierarchy(const SceneHierarchy&,std::string&);
bool validate_scene_hierarchy_shard(const SceneHierarchy&,const SceneHierarchyShard&,std::string&);
bool prepare_scene_hierarchy(const SceneHierarchyRequest&,SceneHierarchyProgress&,std::string&,
    const std::atomic_bool* cancel=nullptr,SceneHierarchyNotify notify={});
// Resolves a confined package-relative cache/shard name; never a source path.
bool scene_hierarchy_path(const std::filesystem::path& package,const std::string& relative,
    std::filesystem::path&,std::string&);
}
