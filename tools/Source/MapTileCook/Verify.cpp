#include "TileCook.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using namespace octaryn::tools::tiles;
namespace {
using Signature=std::array<std::uint64_t,2>;
void hash_float(Signature& hash,float value) {
  const auto bits=std::bit_cast<std::uint32_t>(value==0?0.f:value);
  hash[0]=(hash[0]^bits)*1099511628211ull;hash[1]=(hash[1]+bits)*0x9e3779b185ebca87ull;
}
void append(const MapModel& model,std::vector<Signature>& result) {
  for(size_t i=0;i<model.indices.size();i+=3) {
    Signature hash{14695981039346656037ull,0x517cc1b727220a95ull};
    for(unsigned corner=0;corner<3;++corner) {
      const auto& vertex=model.vertices.at(model.indices[i+corner]);
      for(float value:vertex.position)hash_float(hash,value);
      for(float value:vertex.uv)hash_float(hash,value);
      for(float value:vertex.uv1)hash_float(hash,value);
      for(float value:vertex.color)hash_float(hash,value);
      hash_float(hash,vertex.tangent[3]);
    }
    result.push_back(hash);
  }
}
}
bool verify_map_tiles(const std::filesystem::path& source,const std::filesystem::path& output) {
  MapModel model;std::string error;if(!load_map_model(source,model,error))throw std::runtime_error(error);
  std::vector<Signature> expected,actual;expected.reserve(model.indices.size()/3);append(model,expected);
  model={};std::sort(expected.begin(),expected.end());unsigned count=0,maximum=0;
  std::map<std::string,std::string> identities;
  for(const auto& file:std::filesystem::directory_iterator(output/"tiles")) {
    if(file.path().extension()!=".glb")continue;
    if(!load_map_model(file.path(),model,error))throw std::runtime_error(file.path().string()+": "+error);
    if(model.indices.size()/3>16384)throw std::runtime_error("tile triangle limit exceeded");
    const auto hash=map_texture_file_digest(file.path(),error);if(hash.empty())throw std::runtime_error(error);
    identities.emplace("tiles/"+file.path().filename().generic_string(),hash);
    maximum=std::max(maximum,static_cast<unsigned>(model.indices.size()/3));append(model,actual);model={};++count;
  }
  std::sort(actual.begin(),actual.end());
  if(expected!=actual)throw std::runtime_error("oriented triangle positions/UVs/colors/handedness do not match source");
  const auto source_hash=map_texture_file_digest(source,error);if(source_hash.empty())throw std::runtime_error(error);
  auto receipt=json_stream();receipt<<"{\"version\":1,\"source_sha256\":\""<<source_hash
      <<"\",\"triangles\":"<<actual.size()<<",\"maximum_tile_triangles\":"<<maximum
      <<",\"scope\":\"positions_uv_colors_winding_dual64\",\"files\":{";
  unsigned entry=0;for(const auto& [path,hash]:identities)receipt<<(entry++?",":"")<<'"'<<path<<"\":\""<<hash<<'"';
  receipt<<"}}\n";
  std::ofstream verified(output/"verify.json",std::ios::binary);verified<<receipt.str();verified.close();
  if(!verified)throw std::runtime_error("tile verification receipt write failed");
  std::printf("map_tile_verify passed=1 tiles=%u triangles=%zu maximum=%u positions_uv_colors_winding=exact_dual64_digest\n",count,actual.size(),maximum);return true;
}
