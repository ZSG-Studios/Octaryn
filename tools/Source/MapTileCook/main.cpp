#include "TileCook.h"
#include "MapTextureCache.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using namespace octaryn::tools::tiles;
bool test_map_tiles(const std::filesystem::path&);
bool test_map_tile_order(const std::filesystem::path&);
bool verify_map_tiles(const std::filesystem::path&,const std::filesystem::path&);
int main(int argc,char** argv) {
  try {
    if(argc==3 && std::string(argv[1])=="--self-test")return test_map_tiles(argv[2]) && test_map_tile_order(argv[2])?0:1;
    if(argc==4 && std::string(argv[1])=="--compare")return compare_tile_cooks(argv[2],argv[3])?0:1;
    if(argc==5 && std::string(argv[1])=="--compare-source") {
      if(!std::filesystem::is_regular_file(argv[2]) || std::filesystem::path(argv[2]).extension()!=".glb")
        throw std::runtime_error("source comparison requires a GLB source file");
      return compare_tile_cooks(argv[2],argv[3],argv[4])?0:1;
    }
    if(argc==4 && std::string(argv[1])=="--verify")return verify_map_tiles(argv[2],argv[3])?0:1;
    if(argc!=9 && argc!=11)throw std::runtime_error("usage: map_tile_cook source.glb source.glb.textures output_directory spawn_x spawn_y spawn_z yaw pitch [--order input|morton]");
    Settings settings;
    if(argc==11) {
      if(std::string(argv[9])!="--order" || (std::string(argv[10])!="input" && std::string(argv[10])!="morton"))
        throw std::runtime_error("partition order must be input or morton");
      settings.order=std::string(argv[10])=="morton"?PartitionOrder::Morton:PartitionOrder::Input;
    }
    const char* order=settings.order==PartitionOrder::Morton?"morton":"input";
    const std::filesystem::path source=argv[1],cache=argv[2],output=argv[3];std::string error;
    if(settings.order==PartitionOrder::Morton && std::filesystem::exists(output) && !std::filesystem::is_empty(output))
      throw std::runtime_error("Morton candidate requires an empty separate output directory");
    std::array<float,5> view{};for(unsigned i=0;i<5;++i) {view[i]=std::stof(argv[i+4]);if(!std::isfinite(view[i]))throw std::runtime_error("invalid spawn");}
    MapModel model;if(!load_map_model(source,model,error))throw std::runtime_error(error);
    const auto hash=map_texture_file_digest(source,error);if(hash.empty())throw std::runtime_error(error);
    TextureFiles textures;if(!prepare_textures(model,cache,output,textures,error))throw std::runtime_error(error);
    for(auto& image:model.images)std::vector<std::uint8_t>().swap(image.bytes);
    std::vector<TileResult> tiles;
    if(!partition(model,textures,settings,output,tiles,error))throw std::runtime_error(error);
    auto manifest=json_stream();manifest<<"{\"version\":1,\"map\":\""<<tiles.front().file<<"\",\"spawn\":["<<view[0]<<','<<view[1]<<','<<view[2]
        <<"],\"yaw\":"<<view[3]<<",\"pitch\":"<<view[4]<<",\"texture_cache\":\"textures\",\"tiles\":[";
    std::uint64_t triangles=0;unsigned maximum=0;auto bounds=tiles.front().bounds;
    for(size_t i=0;i<tiles.size();++i) {
      if(i)manifest<<',';manifest<<'[';for(unsigned axis=0;axis<6;++axis)manifest<<(axis?",":"")<<tiles[i].bounds[axis];manifest<<']';
      triangles+=tiles[i].triangles;maximum=std::max(maximum,tiles[i].triangles);
      for(unsigned axis=0;axis<3;++axis) {bounds[axis]=std::min(bounds[axis],tiles[i].bounds[axis]);bounds[axis+3]=std::max(bounds[axis+3],tiles[i].bounds[axis+3]);}
    }
    manifest<<"],\"tile_files\":[";for(size_t i=0;i<tiles.size();++i)manifest<<(i?",":"")<<'"'<<tiles[i].file<<'"';manifest<<"]}\n";
    if(triangles!=model.indices.size()/3 || manifest.str().size()>1024u*1024u)throw std::runtime_error("tile conservation or manifest size check failed");
    std::ofstream stream(output/"map.json",std::ios::binary);stream<<manifest.str();stream.close();if(!stream)throw std::runtime_error("manifest write failed");
    auto metadata=json_stream();metadata<<"{\"version\":1,\"partition_order\":\""<<order<<"\",\"partition_version\":1,\"cell_metres\":"<<settings.cell<<",\"triangle_limit\":"<<settings.triangles<<",\"texture_budget_bytes\":"<<settings.texture_budget<<",\"source_sha256\":\""<<hash<<"\",\"triangles\":"<<triangles<<",\"tiles\":"<<tiles.size()
        <<",\"maximum_tile_triangles\":"<<maximum<<",\"texture_variants\":"<<textures.bytes.size()<<",\"bounds\":[";
    for(unsigned axis=0;axis<6;++axis)metadata<<(axis?",":"")<<bounds[axis];metadata<<"]}\n";
    std::ofstream info(output/"cook.json",std::ios::binary);info<<metadata.str();info.close();if(!info)throw std::runtime_error("metadata write failed");
    std::printf("map_tile_cook complete=1 tiles=%zu triangles=%llu maximum_tile_triangles=%u shared_textures=%zu order=%s\n",tiles.size(),triangles,maximum,textures.bytes.size(),order);return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"map_tile_cook_failed: %s\n",error.what());return 1;}
}
