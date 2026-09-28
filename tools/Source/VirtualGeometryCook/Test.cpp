#include "GeometryCook.h"
#include "GeometryCache.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <cstdio>

using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void check(bool value,const std::string& message) {if(!value)throw std::runtime_error(message);}
MapModel fixture() {
  MapModel model;constexpr unsigned side=32;
  for(unsigned material=0;material<3;++material) {
    const auto base=unsigned(model.vertices.size());MapPrimitive primitive;
    primitive.first_index=unsigned(model.indices.size());primitive.material.alpha_mode=MapAlphaMode(material);
    primitive.material.double_sided=material==1;
    for(unsigned y=0;y<=side;++y)for(unsigned x=0;x<=side;++x) {
      MapVertex v{};v.position[0]=float(x)+float(material)*side;v.position[1]=std::sin(float(x)*.2f)*std::sin(float(y)*.3f);
      v.position[2]=float(y);v.normal[1]=1;v.uv[0]=float(x)/side;v.uv[1]=float(y)/side;v.tangent[0]=v.tangent[3]=1;
      model.vertices.push_back(v);
    }
    for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x) {
      const auto a=base+y*(side+1)+x,b=a+1,c=a+side+1,d=c+1;
      model.indices.insert(model.indices.end(),{a,c,b,b,c,d});
    }
    primitive.index_count=unsigned(model.indices.size())-primitive.first_index;model.primitives.push_back(primitive);
  }
  return model;
}
using Triangle=std::array<unsigned,61>;
Triangle triangle(unsigned material,const std::array<MapVertex,3>& vertices) {
  std::array<std::array<unsigned,20>,3> p{};
  for(unsigned i=0;i<3;++i)std::memcpy(p[i].data(),&vertices[i],sizeof(MapVertex));
  const auto first=unsigned(std::min_element(p.begin(),p.end())-p.begin());Triangle output{};output[0]=material;
  for(unsigned i=0;i<3;++i)std::copy(p[(i+first)%3].begin(),p[(i+first)%3].end(),output.begin()+1+i*20);
  return output;
}
void coverage(const MapModel& model,const GeometryAsset& asset) {
  std::map<Triangle,unsigned> expected,actual;
  for(unsigned p=0;p<model.primitives.size();++p) {
    const auto& primitive=model.primitives[p];
    for(unsigned i=0;i<primitive.index_count;i+=3) {
      std::array<MapVertex,3> v;for(unsigned c=0;c<3;++c)v[c]=model.vertices[model.indices[primitive.first_index+i+c]];
      ++expected[triangle(p,v)];
    }
  }
  std::vector<std::vector<std::uint8_t>> pages(asset.pages.size());std::string error;
  for(unsigned p=0;p<pages.size();++p)check(decode_geometry_page(asset,p,pages[p],error),error);
  for(const auto& cluster:asset.clusters)if(cluster.refined_group==invalid_id) {
    const auto& material=model.primitives[cluster.material].material;
    check(cluster.flags==(unsigned(material.alpha_mode)|(material.double_sided?256u:0u)),"material alpha/double-sided flags changed");
    const auto& data=pages[cluster.page];
    for(unsigned i=0;i<cluster.triangle_count;++i) {
      unsigned packed;std::memcpy(&packed,data.data()+cluster.triangle_offset+i*4,4);std::array<MapVertex,3> v;
      for(unsigned c=0;c<3;++c)std::memcpy(&v[c],data.data()+cluster.vertex_offset+((packed>>(c*8))&255)*sizeof(MapVertex),sizeof(MapVertex));
      ++actual[triangle(cluster.material,v)];
    }
  }
  check(expected==actual,"leaf topology/material/winding multiplicity changed");
}
void flip(const std::filesystem::path& path,std::uint64_t offset) {
  std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.seekg(std::streamoff(offset));
  char byte{};check(bool(file.read(&byte,1)),"test corrupt read failed");byte^=1;file.seekp(std::streamoff(offset));file.write(&byte,1);
}
}
bool test_virtual_geometry(const std::filesystem::path& base) {
  try {
    const auto directory=base/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(directory);const auto path=directory/"fixture.vgeom";
    const auto model=fixture();GeometryAsset asset;std::string error;
    const auto hash=map_texture_digest(std::span<const std::uint8_t>{});
    check(cook_geometry(model,hash,asset,error),error);coverage(model,asset);
    check(std::any_of(asset.groups.begin(),asset.groups.end(),[](const auto& g){return g.depth>0;}),"fixture did not exercise clusterlod hierarchy");
    check(asset.pages.size()>1,"fixture did not exercise page packing");
    std::set<unsigned> root_pages,fine_pages;
    for(const auto& cluster:asset.clusters)
      (std::find(asset.roots.begin(),asset.roots.end(),cluster.group)!=asset.roots.end()?root_pages:fine_pages).insert(cluster.page);
    for(auto page:root_pages)check(!fine_pages.contains(page),"coarse and fine clusters share pinned pages");
    check(std::any_of(asset.pages.begin(),asset.pages.end(),[](const auto& p){return p.codec==GeometryCodec::Meshoptimizer;}),"fixture not compressed");
    auto bad=asset;bad.payloads.front().front()^=1;check(!validate_geometry(bad,error),"corrupted payload accepted");
    bad=asset;bad.clusters.front().page=invalid_id;check(!validate_geometry(bad,error),"bad page reference accepted");
    bad=asset;bad.groups.front().page_count=0;check(!validate_geometry(bad,error),"incomplete group dependencies accepted");
    bad=asset;bad.source_triangles++;check(!validate_geometry(bad,error),"missing leaf coverage accepted");
    bad=asset;bad.roots.clear();check(!validate_geometry(bad,error),"missing roots accepted");
    bad=asset;bad.clusters.back().refined_group=bad.clusters.back().group;check(!validate_geometry(bad,error),"refinement cycle accepted");
    check(write_geometry_cache(path,asset,error),error);GeometryAsset manifest,loaded;
    check(write_geometry_cache(path,asset,error),error);
    check(read_geometry_cache(path,hash,manifest,error,false),error);check(manifest.payloads.empty(),"manifest loaded full geometry");
    for(unsigned i=0;i<manifest.pages.size();++i) {
      std::vector<std::uint8_t> disk,memory;check(read_geometry_page(path,manifest.pages[i],disk,error),error);
      check(decode_geometry_page(asset,i,memory,error),error);check(disk==memory,"independent page roundtrip mismatch");
    }
    check(read_geometry_cache(path,hash,loaded,error),error);coverage(model,loaded);
    check(!read_geometry_cache(path,std::string(64,'f'),loaded,error),"wrong source accepted");
    flip(path,manifest.pages.front().file_offset);std::vector<std::uint8_t> decoded;
    check(!read_geometry_page(path,manifest.pages.front(),decoded,error),"disk corruption accepted");
    flip(path,manifest.pages.front().file_offset);flip(path,180);
    check(!read_geometry_cache(path,hash,loaded,error,false),"metadata corruption accepted");
    flip(path,180);
    std::printf("virtual_geometry_self_test passed=1 exact_leaf_coverage=1 hierarchy=1 materials=3 independent_pages=1 corruption=1 clusters=%zu groups=%zu pages=%zu\n",
        asset.clusters.size(),asset.groups.size(),asset.pages.size());return true;
  }catch(const std::exception& failure) {std::fprintf(stderr,"virtual_geometry_self_test_failed: %s\n",failure.what());return false;}
}
