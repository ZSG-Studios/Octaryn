#include "ScenePreparation.h"
#include "SceneCatalog.h"
#include <cstdio>
#include <chrono>
#include <stdexcept>

void test_scene_preparation(const std::filesystem::path& root,const std::filesystem::path& source) {
  using namespace octaryn::client::rendering::virtual_geometry;
  const auto require=[](bool value,const std::string& error){if(!value)throw std::runtime_error(error);};
  ScenePreparationRequest request;request.source=source;request.catalog=root/"preparation"/"scene.json";
  request.camera=request.actor={10,0,0};request.render_radius=4;request.collision_radius=2;
  ScenePreparationResult result;std::string error;std::atomic_bool canceled{};
  SceneCatalog original;require(import_scene_catalog(source,original,error) && write_scene_catalog(request.catalog,original,error),error);
  const auto notify=[&](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Bounds && p.completed)canceled=true;
  };
  require(!prepare_scene_neighborhood(request,result,error,&canceled,notify) && result.canceled,"scene preparation did not cancel");
  SceneCatalog catalog;require(read_scene_catalog(request.catalog,catalog,error) && catalog.parts[0].bounds_prepared &&
      catalog.parts[0].geometry.empty(),"cancellation lost checkpoint or published uncooked geometry");
  canceled=false;request.collision_budget_bytes=1;
  require(!prepare_scene_neighborhood(request,result,error,&canceled) && result.collision_pairs==1 && result.collision_bytes>1 &&
      !result.neighborhood_ready,"protected collision budget silently dropped original nodes");
  request.collision_budget_bytes=512ull<<20;request.gpu_budget_bytes=1;
  require(!prepare_scene_neighborhood(request,result,error,&canceled) && result.render_bytes>1 && !result.neighborhood_ready,
      "render budget silently dropped scene parts");
  require(read_scene_catalog(request.catalog,catalog,error) && catalog.parts[0].geometry.empty(),
      "render minimum admission started cooking before rejecting the neighborhood");
  request.gpu_budget_bytes=512ull<<20;
  require(prepare_scene_neighborhood(request,result,error,&canceled) && result.neighborhood_ready && result.render_parts==1 &&
      result.collision_pairs==1,error);
  require(read_scene_catalog(request.catalog,catalog,error) && catalog.instances.size()==2 && catalog.source_hash==original.source_hash,
      "local preparation altered complete scene identity");
  const auto cache=request.catalog.parent_path()/catalog.parts[0].geometry;const auto stamp=std::filesystem::last_write_time(cache);
  require(prepare_scene_neighborhood(request,result,error,&canceled) && std::filesystem::last_write_time(cache)==stamp,
      "resumed preparation rewrote valid geometry");
  const auto source_stamp=std::filesystem::last_write_time(source);
  const auto mutation=[&](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Geometry && p.completed)
      std::filesystem::last_write_time(source,source_stamp+std::chrono::seconds(1));
  };
  const bool accepted=prepare_scene_neighborhood(request,result,error,&canceled,mutation);
  std::filesystem::last_write_time(source,source_stamp);
  require(!accepted && error.find("source changed")!=std::string::npos,"preparation published after source mutation");
  std::puts("scene_preparation_tests passed=1 cancellation_checkpoint=1 resume=1 complete_neighborhood_admission=1 source_identity=1 source_mutation_rejected=1");
}
