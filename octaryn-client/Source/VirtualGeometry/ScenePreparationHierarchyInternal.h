#pragma once
#include "ScenePreparationWorld.h"
#include "SceneHierarchy.h"

namespace octaryn::client::rendering::virtual_geometry {
std::filesystem::path prepare_world_layout(const ScenePreparationRequest&,SceneCatalog&,const std::atomic_bool*,const ScenePreparationNotify&);
void prepare_world_hierarchy(const ScenePreparationRequest&,SceneCatalog&,ScenePreparationWorldResult&,
    const std::atomic_bool*,const ScenePreparationNotify&,std::uint32_t target_triangles);
void validate_world_collision(const ScenePreparationRequest&,const SceneCatalog&,ScenePreparationResult&);
void prepare_world_collision(const ScenePreparationRequest&,SceneCatalog&,ScenePreparationResult&,
    const std::atomic_bool*,const ScenePreparationNotify&);
}
