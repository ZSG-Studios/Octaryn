#include "Simplify.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace octaryn::client::rendering {
bool test_map_asset_prepare(const std::filesystem::path&);
bool test_map_lods(const std::filesystem::path& root) {
  const auto require=[](bool okay,const char* reason) {if(!okay)throw std::runtime_error(reason);};
  MapModel model;constexpr unsigned width=17;
  for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x) {
    MapVertex vertex{};vertex.position[0]=float(x);vertex.position[2]=float(y);vertex.normal[1]=1;
    vertex.uv[0]=x/16.f;vertex.uv[1]=y/16.f;model.vertices.push_back(vertex);
  }
  for(unsigned y=0;y<width-1;++y)for(unsigned x=0;x<width-1;++x) {
    const unsigned a=y*width+x,b=a+1,c=a+width,d=c+1;
    for(auto i:{a,c,b,b,c,d})model.indices.push_back(i);
  }
  MapPrimitive primitive{};primitive.index_count=static_cast<unsigned>(model.indices.size());model.primitives.push_back(primitive);
  const auto original=model.indices;const auto lods=cook_map_lods(model);
  require(model.indices==original,"original indices modified");
  require(lods.primitives[0][0].count>0 && lods.primitives[0][0].count<original.size(),"planar LOD reduction");
  for(const auto& level:lods.primitives[0]) {
    require(std::isfinite(level.error) && level.error>=0,"invalid error bound");
    for(unsigned v=0;v<width*width;++v)if(v%width==0 || v%width==width-1 || v<width || v>=width*(width-1))
      require(std::find(lods.indices.begin()+level.first,lods.indices.begin()+level.first+level.count,v)!=
          lods.indices.begin()+level.first+level.count,"primitive boundary vertex removed");
  }
  std::filesystem::create_directories(root);const auto path=root/"fixture.lods";const std::string hash(64,'a');
  std::string error;MapLodData read;
  require(write_map_lods(path,hash,model,lods,error),"LOD cache write");
  require(read_map_lods(path,hash,model,read,error) && read.indices==lods.indices,"LOD roundtrip");
  require(!read_map_lods(path,std::string(64,'b'),model,read,error),"stale source accepted");
  auto bad=lods;bad.indices.front()=static_cast<unsigned>(model.vertices.size());
  require(!write_map_lods(root/"bad.lods",hash,model,bad,error),"foreign primitive index accepted");
  std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);file.seekp(120);file.put(127);file.close();
  require(!read_map_lods(path,hash,model,read,error),"corrupt cache accepted");
  model.primitives[0].material.alpha_mode=MapAlphaMode::Mask;
  require(cook_map_lods(model).indices.empty(),"masked foliage simplified");
  std::printf("map_lod_tests passed=1\n");return test_map_asset_prepare(root);
}
}
