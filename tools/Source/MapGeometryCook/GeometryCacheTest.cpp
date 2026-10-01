#include "MapAssetPrepare.h"
#include "../../../octaryn-client/Source/VirtualGeometry/GeometryCache.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <future>
#include <stdexcept>

namespace octaryn::client::rendering {
bool test_virtual_map_cache(const std::filesystem::path& root) {
  using namespace virtual_geometry;
  const auto require=[](bool value,const std::string& reason) {if(!value)throw std::runtime_error(reason);};
  constexpr auto variable="OCTARYN_CLIENT_GEOMETRY_CACHE_PATH";
  struct Environment {
    std::string previous;bool existed{};
    ~Environment() {
      if(existed)SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"OCTARYN_CLIENT_GEOMETRY_CACHE_PATH",previous.c_str(),true);
      else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"OCTARYN_CLIENT_GEOMETRY_CACHE_PATH");
    }
  } environment;
  if(const auto* previous=SDL_getenv(variable)) {environment.previous=previous;environment.existed=true;}
  const auto directory=std::filesystem::absolute(root/"virtual-cache");
  const auto version_directory=directory/("v"+std::to_string(geometry_version));
  const auto utf8=directory.generic_u8string();
  require(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),variable,reinterpret_cast<const char*>(utf8.c_str()),true),"cache environment");
  MapModel model;model.vertices.resize(3);model.indices={0,1,2};model.primitives.resize(1);
  model.vertices[1].position[0]=1;model.vertices[2].position[2]=1;
  for(auto& vertex:model.vertices)vertex.normal[1]=1;
  model.primitives[0].index_count=3;
  const auto source=root/"read-only-source"/"external.gltf";
  MapGeometryCache first;std::string error;
  require(prepare_map_geometry(source,model,first,error),error);
  require(first.path.parent_path()==version_directory && !std::filesystem::exists(source.parent_path()),"cache modified import source directory");
  const auto modified=std::filesystem::last_write_time(first.path);
  MapGeometryCache reused;
  require(prepare_map_geometry(source,model,reused,error) && reused.path==first.path &&
      std::filesystem::last_write_time(first.path)==modified,"valid content cache was replaced");
  auto changed=model;changed.vertices[1].position[0]=2;
  MapGeometryCache external;
  require(prepare_map_geometry(source,changed,external,error) && external.hash!=first.hash,"external geometry change reused stale cache");
  changed=model;changed.primitives[0].material.alpha_mode=MapAlphaMode::Mask;
  MapGeometryCache coverage;
  require(prepare_map_geometry(source,changed,coverage,error) && coverage.hash!=first.hash,"alpha coverage change reused stale cache");
  {std::ofstream corrupt(first.path,std::ios::binary|std::ios::trunc);corrupt<<"invalid";}
  const auto repair=[&] {
    MapGeometryCache cache;std::string failure;
    if(!prepare_map_geometry(source,model,cache,failure))throw std::runtime_error(failure);
    return cache;
  };
  auto worker=std::async(std::launch::async,repair);auto repaired=repair();const auto concurrent=worker.get();
  GeometryAsset asset;
  require(repaired.path==concurrent.path && read_geometry_cache(repaired.path,repaired.hash,asset,error),"concurrent cache repair failed");
  for(const auto& entry:std::filesystem::directory_iterator(version_directory))
    require(entry.path().extension()==".vgeom","cache left temporary file");
  std::puts("virtual_map_cache_tests passed=1 source_unchanged=1 external_geometry_key=1 material_coverage_key=1 concurrent_repair=1");
  return true;
}
}
