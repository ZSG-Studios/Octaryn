#pragma once
#include "SceneResidency.h"
#include "SpatialTriangleOrder.h"
#include <filesystem>
#include <atomic>

namespace octaryn::character_motion {
struct SceneCollisionOrder {
  std::filesystem::path path;
  scene_geometry::SpatialOrderConfig config;
  std::filesystem::file_time_type stamp{};
  std::uint64_t bytes{};
};
struct SceneCollisionCatalog {
  std::filesystem::path source;
  std::vector<scene_geometry::Part> parts;
  std::vector<scene_geometry::Instance> instances;
  std::vector<SceneCollisionOrder> orders;
  std::vector<std::uint32_t> part_orders;
};
bool read_scene_collision_catalog(const std::filesystem::path& catalog,const std::filesystem::path& source,
    SceneCollisionCatalog&,std::string& error,const std::atomic_bool* cancel=nullptr);
}
