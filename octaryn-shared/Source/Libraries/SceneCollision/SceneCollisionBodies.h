#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace octaryn::character_motion {
struct SceneCollisionCatalog;
bool read_scene_body_exclusions(const std::filesystem::path& physics_catalog,
    std::vector<std::string>& names,std::string& error);
bool exclude_scene_bodies(const std::filesystem::path& source,const std::string& catalog_json,
    SceneCollisionCatalog&,std::string& error);
}
