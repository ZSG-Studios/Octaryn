#include "SceneCook.h"
#include "ScenePreparation.h"
#include <cstdio>
#include <stdexcept>

bool prepare_scene_bounds(const std::filesystem::path& path,std::uint64_t first,std::uint64_t count) {
  using namespace octaryn::client::rendering::virtual_geometry;
  ScenePreparationResult result;std::string error;
  const auto report=[](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Bounds && (p.completed%64==0 || p.completed==p.requested)) {
      std::printf("scene_bounds_progress prepared=%llu requested=%llu total_parts=%llu\n",
          static_cast<unsigned long long>(p.prepared_bounds),static_cast<unsigned long long>(p.requested),
          static_cast<unsigned long long>(p.total_parts));std::fflush(stdout);
    }
  };
  if(!prepare_scene_range(path,first,count,ScenePreparationMode::Bounds,result,error,nullptr,report))throw std::runtime_error(error);
  std::printf("scene_bounds_complete prepared_parts=%llu total_parts=%llu source_expansion=0\n",
      static_cast<unsigned long long>(result.prepared_bounds),static_cast<unsigned long long>(result.total_parts));
  return true;
}
