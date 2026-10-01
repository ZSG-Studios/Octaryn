#pragma once
#include "ScenePreparation.h"

namespace octaryn::client::rendering::virtual_geometry {
struct ScenePreparationWorldResult {
  ScenePreparationResult scene;
  std::filesystem::path catalog,hierarchy;
  std::array<float,3> spawn{};
  std::uint64_t root_triangles{},root_payload_bytes{},representation_bytes{},ray_expansion_bytes{},raster_bytes{};
  bool hierarchy_ready{};
};
// Worlds retain their imported catalog and publish a complete coarse forest and exact collision catalog.
// Device-specific AS sizes still require the renderer's aggregate admission before opening.
bool prepare_scene_world(const ScenePreparationRequest&,ScenePreparationWorldResult&,std::string&,
    const std::atomic_bool* cancel=nullptr,ScenePreparationNotify notify={},std::uint32_t hierarchy_target_triangles=512);
// Validates the complete forest and its original source/layout identity without loading descendants.
bool validate_scene_world_hierarchy(const std::filesystem::path& catalog,const std::filesystem::path& hierarchy,
    std::string& error);
}
