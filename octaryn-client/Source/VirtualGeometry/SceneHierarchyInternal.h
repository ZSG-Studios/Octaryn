#pragma once
#include "SceneHierarchy.h"
#include "ScenePreparationInternal.h"
#include <string_view>

namespace octaryn::client::rendering::virtual_geometry {
std::string hierarchy_digest(std::string_view);
SceneHierarchyGeometry describe_hierarchy_geometry(const std::string&,const GeometryAsset&);
std::string hierarchy_layout_identity(const SceneCatalog&,const SceneHierarchyRequest&);
bool write_scene_hierarchy(const std::filesystem::path&,const SceneHierarchy&,std::string&);
bool write_scene_hierarchy_shard(const std::filesystem::path&,SceneHierarchy&,SceneHierarchyShard&,std::string&);
bool read_hierarchy_work(const std::filesystem::path&,const SceneHierarchy&,std::uint32_t,SceneHierarchyShard&,std::string&);
bool write_hierarchy_work(const std::filesystem::path&,const SceneHierarchy&,const SceneHierarchyShard&,std::string&);
struct SceneHierarchyWork {
  SceneHierarchyRequest request;
  ScenePreparationWork source;
  SceneHierarchy hierarchy;
  SceneHierarchyProgress progress;
  SceneHierarchyNotify notify;
  std::uint64_t new_leaves{};
  struct OrderStamp {std::uint32_t primitive;std::filesystem::path path;std::filesystem::file_time_type time;std::uintmax_t bytes;};
  std::vector<OrderStamp> order_stamps;
  void snapshot_orders();
  void checkpoint();
  void check_snapshot(std::uint32_t primitive=invalid_id);
  void report();
  void prepare_primitive(std::uint32_t);
  SceneHierarchyNode leaf(std::uint32_t primitive,std::uint32_t part);
  SceneHierarchyNode parent(const SceneHierarchyPrimitive&,const SceneHierarchyShard&,std::span<const std::uint32_t>);
};
}
