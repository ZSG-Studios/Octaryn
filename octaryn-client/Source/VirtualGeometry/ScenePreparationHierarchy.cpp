#include "ScenePreparationHierarchyInternal.h"
#include "SceneHierarchyInternal.h"
#include "ScenePreparationLock.h"
#include "ResourceDigest.h"
#include "FilePath.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
void check(const std::atomic_bool* cancel) {
  require(!cancel || !cancel->load(std::memory_order_relaxed),"scene world preparation canceled");
}
SceneCatalog load(const ScenePreparationRequest& request,const std::atomic_bool* cancel) {
  ScenePreparationLock lock(request.catalog);SceneCatalog catalog;std::string error;check(cancel);
  if(std::filesystem::exists(content::file_io_path(request.catalog))) {
    require(read_scene_catalog(request.catalog,catalog,error),error);
    require(std::filesystem::equivalent(content::file_io_path(request.source),content::file_io_path(preparation_path(catalog.source))),
        "scene world catalog belongs to another source");
  }else {
    require(import_scene_catalog(request.source,catalog,error,cancel),error);check(cancel);
    require(write_scene_catalog(request.catalog,catalog,error),error);
  }
  return catalog;
}
}
bool validate_scene_world_hierarchy(const std::filesystem::path& catalog_path,const std::filesystem::path& path,
    std::string& error) {
  try {
    SceneCatalog catalog;SceneHierarchy hierarchy;
    require(read_scene_catalog(catalog_path,catalog,error),error);
    require(read_scene_hierarchy(path,hierarchy,error),error);
    require(hierarchy.complete,"prepared world hierarchy is incomplete");
    SceneHierarchyRequest options;options.target_triangles=hierarchy.target_triangles;
    options.fan_in=hierarchy.fan_in;options.maximum_triangles=hierarchy.maximum_triangles;
    require(hierarchy.identity==hierarchy_layout_identity(catalog,options) && hierarchy.source_hash==catalog.source_hash,
        "prepared world hierarchy differs from original source layout");
    require(std::all_of(catalog.parts.begin(),catalog.parts.end(),[](const auto& p){return p.bounds_prepared;}),
        "prepared world collision bounds are incomplete");
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool prepare_scene_world(const ScenePreparationRequest& request,ScenePreparationWorldResult& result,std::string& error,
    const std::atomic_bool* cancel,ScenePreparationNotify notify,std::uint32_t hierarchy_target_triangles) {
  result={};error.clear();
  try {
    check(cancel);if(notify)notify({ScenePreparationStage::Inspect});
    auto catalog=load(request,cancel);result.catalog=request.catalog;result.scene.total_parts=catalog.parts.size();
    {
      ScenePreparationLock lock(request.catalog);
      const auto canonical_digest=content::resource_file_digest(request.catalog,error,32ull<<20);
      require(!canonical_digest.empty(),error);
      std::vector<std::filesystem::file_time_type> stamps;
      for(const auto& resource:catalog.resources) {
        const auto path=content::file_io_path(preparation_path(resource.path));
        require(std::filesystem::file_size(path)==resource.bytes,"scene world source size changed");
        stamps.push_back(std::filesystem::last_write_time(path));
      }
      auto ordered=request;ordered.catalog=prepare_world_layout(request,catalog,cancel,notify);
      prepare_world_collision(ordered,catalog,result.scene,cancel,notify);
      prepare_world_hierarchy(ordered,catalog,result,cancel,notify,hierarchy_target_triangles);check(cancel);
      validate_world_collision(request,catalog,result.scene);
      result.catalog=request.catalog.parent_path()/"world-scene.json";
      require(write_scene_catalog(result.catalog,catalog,error),error);
      if(notify)notify({ScenePreparationStage::Spawn});
      require(qualify_scene_spawn(result.catalog,request.source,request.actor,result.spawn,error,cancel),error);
      auto settled=request;settled.actor=settled.camera=result.spawn;
      validate_world_collision(settled,catalog,result.scene);check(cancel);
      for(std::size_t i=0;i<catalog.resources.size();++i) {
        const auto path=content::file_io_path(preparation_path(catalog.resources[i].path));
        require(std::filesystem::file_size(path)==catalog.resources[i].bytes && std::filesystem::last_write_time(path)==stamps[i],
            "scene world source changed before publication");
      }
      require(content::resource_file_digest(request.catalog,error,32ull<<20)==canonical_digest,
          "scene world canonical catalog changed during preparation");
      result.hierarchy_ready=true;result.scene.neighborhood_ready=true;result.scene.full_scene_ready=true;
    }
    check(cancel);if(notify)notify({ScenePreparationStage::Ready,result.scene.total_parts,result.scene.total_parts,
        result.scene.total_parts,result.scene.prepared_bounds,result.scene.cooked_parts});
    return true;
  }catch(const std::exception& failure) {
    error=failure.what();result.scene.neighborhood_ready=false;result.hierarchy_ready=false;
    result.scene.canceled=cancel && cancel->load(std::memory_order_relaxed);
    if(notify)notify({result.scene.canceled?ScenePreparationStage::Canceled:ScenePreparationStage::Failed});
    return false;
  }
}
}
