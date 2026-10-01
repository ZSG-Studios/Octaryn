#include "GeometryCoarse.h"
#include "GeometryCook.h"
#include "GeometryCache.h"
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool value,const std::string& message) {if(!value)throw std::runtime_error(message);}
}
void test_coarse_faces(const std::filesystem::path& root) {
  using namespace octaryn::client::rendering;
  using namespace octaryn::client::rendering::virtual_geometry;
  for(unsigned test=0;test<4;++test) {
    MapModel model;model.vertices.resize(3);
    model.vertices[1].position[0]=2;model.vertices[2].position[2]=2;
    for(auto& vertex:model.vertices)vertex.normal[1]=1;
    model.vertices[1].uv[0]=1;model.vertices[2].uv[1]=1;
    model.indices={0,1,2,1,2,0,0,2,1};
    model.primitives.emplace_back();auto& draw=model.primitives[0];
    draw.material.alpha_mode=test==2?MapAlphaMode::Blend:(test==1 || test==3?MapAlphaMode::Mask:MapAlphaMode::Opaque);
    if(test==3) {
      for(unsigned i=0;i<3;++i) {auto vertex=model.vertices[i];vertex.uv[0]+=.125f;vertex.normal[1]=-1;model.vertices.push_back(vertex);}
      model.indices.insert(model.indices.end(),{3,4,5});
    }
    draw.index_count=unsigned(model.indices.size());const auto source_triangles=model.indices.size()/3;
    GeometryAsset coarse,fine;std::string error;const auto key=std::string(64,char('a'+test));
    require(cook_coarse_geometry(model,key,coarse,error),error);
    const std::uint64_t expected=test<2?2:3;
    require(coarse.source_triangles==expected,"coarse exact-face filtering changed orientation, authored attributes or BLEND multiplicity");
    require(cook_geometry(model,key,fine,error),error);
    require(fine.source_triangles==source_triangles,"coarse filtering changed exact full-detail source triangle counts");
    const auto file=root/("coarse-faces-"+std::to_string(test)+".vgeom");
    require(write_geometry_cache(file,coarse,error),error);
    require(read_geometry_cache(file,key,coarse,error,true),error);
    MapModel decoded;decoded.primitives.emplace_back();decoded.primitives[0].material=draw.material;
    require(append_geometry_roots(file,coarse,decoded,262144,error),error);
    unsigned forward{},reverse{},seams{};
    for(std::size_t i=0;i<decoded.indices.size();i+=3) {
      const auto& a=decoded.vertices.at(decoded.indices[i]);const auto& b=decoded.vertices.at(decoded.indices[i+1]);
      const auto& c=decoded.vertices.at(decoded.indices[i+2]);
      const auto winding=(b.position[0]-a.position[0])*(c.position[2]-a.position[2])-(b.position[2]-a.position[2])*(c.position[0]-a.position[0]);
      forward+=winding>0;reverse+=winding<0;
      seams+=a.normal[1]==-1 && b.normal[1]==-1 && c.normal[1]==-1 && a.uv[0]>=.125f;
    }
    require(reverse==1 && forward==(test>=2?2u:1u),"coarse filtering removed an opposite-winding surface");
    require(test!=3 || seams==1,"coarse MASK filtering changed authored UV/normal seam");
    for(const auto& cluster:coarse.clusters)require((cluster.flags&3u)==unsigned(draw.material.alpha_mode),"coarse face filtering changed material mode");
  }
  std::printf("scene_coarse_face_filter_tests passed=1 opaque=1 mask=1 reverse_winding=1 authored_uv_normal_seams=1 blend_multiplicity=1 exact_leaf_counts=1\n");
}
