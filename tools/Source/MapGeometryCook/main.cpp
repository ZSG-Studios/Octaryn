#include "Simplify.h"
#include "MapMeshOptimization.h"
#include "MapTextureCache.h"
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
namespace octaryn::client::rendering {bool cook_map_tile_lods(const std::filesystem::path&);}
int main(int argc,char** argv) {
  try {
    if(argc==3 && std::string(argv[1])=="--self-test")return test_map_lods(argv[2])?0:1;
    if(argc==3 && std::string(argv[1])=="--tiles")return cook_map_tile_lods(argv[2])?0:1;
    if(argc!=3)throw std::runtime_error("usage: map_geometry_cook source.glb output.glb.lods");
    const std::filesystem::path source=argv[1],output=argv[2];std::string error;
    MapModel model;
    if(!load_map_model(source,model,error) || !optimize_map_mesh(model,error))throw std::runtime_error(error);
    for(auto& image:model.images)std::vector<std::uint8_t>().swap(image.bytes);
    const auto hash=map_texture_file_digest(source,error);if(hash.empty())throw std::runtime_error(error);
    auto lods=cook_map_lods(model);std::filesystem::create_directories(output.parent_path());
    if(!write_map_lods(output,hash,model,lods,error))throw std::runtime_error(error);
    MapLodData verified;if(!read_map_lods(output,hash,model,verified,error))throw std::runtime_error(error);
    std::printf("map_geometry_cook complete=1 verified=1\n");return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"map_geometry_cook_failed: %s\n",error.what());return 1;}
}
