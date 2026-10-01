#include "SceneCook.h"
#include "SceneCatalog.h"
#include "ScenePreparationLock.h"
#include "ScenePreparation.h"
#include "SceneOrder.h"
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string_view>

int prepare_scene_hierarchy_cli(int,char**);
int test_scene_world_preparation(const std::filesystem::path&);

namespace {
std::uint64_t number(const char* input) {
  std::uint64_t value{};const std::string_view text(input);
  const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
  if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())throw std::runtime_error("invalid nonnegative part index/count");
  return value;
}
}
int main(int argc,char** argv) {
  using namespace octaryn::client::rendering::virtual_geometry;
  try {
    if(argc==3 && std::string_view(argv[1])=="--world-test")
      return test_scene_world_preparation(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2])));
    if(argc>=2 && (std::string_view(argv[1])=="--hierarchy" || std::string_view(argv[1])=="--hierarchy-test" ||
        std::string_view(argv[1])=="--hierarchy-window" || std::string_view(argv[1])=="--hierarchy-audit"))return prepare_scene_hierarchy_cli(argc,argv);
    if((argc==7 || argc==9) && std::string_view(argv[1])=="--prepare")return prepare_scene_neighborhood_cli(argc,argv);
    if(argc==7 && std::string_view(argv[1])=="--spawn")return qualify_scene_spawn_cli(argv);
    if(argc==4 && std::string_view(argv[1])=="--catalog") {
      SceneCatalog catalog;std::string error;
      const auto source=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
      const auto output=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[3]));
      ScenePreparationLock lock(output);
      if(!import_scene_catalog(source,catalog,error) || !write_scene_catalog(output,catalog,error))throw std::runtime_error(error);
      std::puts("scene_catalog_saved source_complete=1 geometry_ready=0");return 0;
    }
    if((argc==3 || argc==5) && std::string_view(argv[1])=="--cook") {
      const auto catalog=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
      return cook_scene_parts(catalog,
          argc==5?number(argv[3]):0,argc==5?number(argv[4]):UINT64_MAX)?0:1;
    }
    if((argc==3 || argc==5) && std::string_view(argv[1])=="--bounds") {
      const auto catalog=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
      return prepare_scene_bounds(catalog,argc==5?number(argv[3]):0,argc==5?number(argv[4]):UINT64_MAX)?0:1;
    }
    if((argc==3 || argc==5) && std::string_view(argv[1])=="--order") {
      std::string error;const auto catalog=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
      if(!prepare_scene_spatial_order(catalog,argc==5?number(argv[3]):0,argc==5?number(argv[4]):UINT64_MAX,error))
        throw std::runtime_error(error);
      return 0;
    }
    if(argc==3 && std::string_view(argv[1])=="--self-test")
      return test_scene_catalog(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2])))?0:1;
    throw std::runtime_error("usage: map_scene_cook --catalog source.gltf catalog.json | --cook catalog.json [first_part part_count] | --bounds catalog.json [first_part part_count] | --order catalog.json [first_primitive primitive_count] | --prepare source.gltf catalog.json x y z | --self-test directory");
  }catch(const std::exception& failure) {std::fprintf(stderr,"scene_cook_failed reason=%s\n",failure.what());return 1;}
}
