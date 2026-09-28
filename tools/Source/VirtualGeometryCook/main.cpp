#include "GeometryCook.h"
#include "GeometryCache.h"
#include "MapTextureCache.h"
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
bool test_virtual_geometry(const std::filesystem::path&);
int main(int argc,char** argv) {
  try {
    if(argc==3 && std::string_view(argv[1])=="--self-test")return test_virtual_geometry(argv[2])?0:1;
    if(argc!=3)throw std::runtime_error("usage: virtual_geometry_cook source.glb output.vgeom | --self-test output-directory");
    MapModel model;GeometryAsset asset;std::string error;
    const auto hash=map_texture_file_digest(argv[1],error);
    if(hash.empty() || !load_map_model(argv[1],model,error))throw std::runtime_error(error);
    std::vector<MapModelImage>().swap(model.images);
    if(!cook_geometry(model,hash,asset,error) || !write_geometry_cache(argv[2],asset,error))throw std::runtime_error(error);
    GeometryAsset verified;
    if(!read_geometry_cache(argv[2],hash,verified,error))throw std::runtime_error(error);
    std::printf("virtual_geometry_cook verified=1 static_world_space=1 clusters=%zu groups=%zu pages=%zu roots=%zu triangles=%llu\n",
        verified.clusters.size(),verified.groups.size(),verified.pages.size(),verified.roots.size(),
        static_cast<unsigned long long>(verified.source_triangles));return 0;
  }catch(const std::exception& failure) {std::fprintf(stderr,"virtual_geometry_cook_failed: %s\n",failure.what());return 1;}
}
