#include "SceneCook.h"
#include "ScenePreparation.h"
#include <cstdio>
#include <stdexcept>

bool cook_scene_parts(const std::filesystem::path& path,std::uint64_t first,std::uint64_t count) {
  using namespace octaryn::client::rendering::virtual_geometry;
  ScenePreparationResult result;std::string error;
  const auto report=[](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Geometry) {
      std::printf("scene_cook_progress completed=%llu requested=%llu ready_parts=%llu total_parts=%llu\n",
          static_cast<unsigned long long>(p.completed),static_cast<unsigned long long>(p.requested),
          static_cast<unsigned long long>(p.cooked_parts),static_cast<unsigned long long>(p.total_parts));std::fflush(stdout);
    }
  };
  if(!prepare_scene_range(path,first,count,ScenePreparationMode::Geometry,result,error,nullptr,report))throw std::runtime_error(error);
  std::printf("scene_cook_complete ready_parts=%llu total_parts=%llu full_scene_ready=%u source_expansion=0\n",
      static_cast<unsigned long long>(result.cooked_parts),static_cast<unsigned long long>(result.total_parts),unsigned(result.full_scene_ready));
  return true;
}
