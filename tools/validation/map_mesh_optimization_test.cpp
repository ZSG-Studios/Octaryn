#include "MapMeshOptimization.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void require(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
std::vector<std::string> triangles(const MapModel& model,const MapPrimitive& primitive) {
  std::vector<std::string> result;
  for(unsigned i=primitive.first_index;i<primitive.first_index+primitive.index_count;i+=3) {
    std::string key;
    for(unsigned j=0;j<3;++j) {
      const auto& v=model.vertices[model.indices[i+j]];
      key.append(reinterpret_cast<const char*>(&v),sizeof(v));
    }
    result.push_back(std::move(key));
  }
  return result;
}
MapModel fixture() {
  MapModel model;
  constexpr unsigned corners[]{0,1,2,0,2,3};
  for(unsigned primitive=0;primitive<2;++primitive) {
    MapPrimitive p{};p.first_index=unsigned(model.indices.size());p.index_count=6;
    p.material.base_color[primitive]=.3f;p.material.alpha_mode=primitive?MapAlphaMode::Blend:MapAlphaMode::Opaque;
    model.primitives.push_back(p);
    for(auto corner:corners) {
      MapVertex v{};v.position[0]=float(corner==1||corner==2)+primitive*3;
      v.position[1]=float(corner>=2);v.normal[2]=1;v.tangent[0]=v.tangent[3]=1;
      v.uv[0]=v.position[0];v.uv[1]=v.position[1];v.uv1[0]=.25;v.color[2]=.7;
      model.indices.push_back(unsigned(model.vertices.size()));model.vertices.push_back(v);
    }
  }
  return model;
}
void parity(const MapModel& before,const MapModel& after) {
  require(before.indices.size()==after.indices.size(),"triangle count changed");
  require(before.primitives.size()==after.primitives.size(),"primitive count changed");
  for(unsigned i=0;i<before.primitives.size();++i) {
    const auto& a=before.primitives[i];const auto& b=after.primitives[i];
    require(a.first_index==b.first_index&&a.index_count==b.index_count,"primitive range changed");
    require(a.material.alpha_mode==b.material.alpha_mode&&
        std::memcmp(a.material.base_color,b.material.base_color,sizeof(a.material.base_color))==0,"material changed");
    auto ta=triangles(before,a),tb=triangles(after,b);
    if(a.material.alpha_mode!=MapAlphaMode::Blend) {std::sort(ta.begin(),ta.end());std::sort(tb.begin(),tb.end());}
    require(ta==tb,"oriented full-attribute triangles or blend ordering changed");
  }
}
}
int main() {
  try {
    std::string error;
    for(unsigned seam=0;seam<6;++seam) {
      auto model=fixture();auto& v=model.vertices[3];
      if(seam==1)v.normal[0]=.1f;
      if(seam==2)v.uv[0]=.1f;
      if(seam==3)v.uv1[1]=.1f;
      if(seam==4)v.tangent[3]=-1;
      if(seam==5)v.color[0]=.1f;
      const auto before=model;
      require(optimize_map_mesh(model,error),error.c_str());parity(before,model);
      require(model.vertices.size()==(seam?9:8),"exact attribute seam merged or duplicate retained");
      const auto optimized=model;
      require(optimize_map_mesh(model,error),error.c_str());parity(optimized,model);
    }
    auto special=fixture();special.primitives.resize(1);
    special.indices={0,1,2,0,1,2,2,1,0,0,0,1};special.primitives[0].index_count=12;
    const auto before=special;
    require(optimize_map_mesh(special,error),error.c_str());parity(before,special);
    auto invalid=fixture();invalid.indices[0]=999;
    require(!optimize_map_mesh(invalid,error),"out-of-range index accepted");
    require(invalid.indices[0]==999&&invalid.vertices.size()==12,"failed optimization mutated input");
    invalid=fixture();invalid.primitives[1].first_index=3;
    require(!optimize_map_mesh(invalid,error),"overlapping primitive ranges accepted");
    std::printf("map_mesh_optimization=passed checks=%u exact_attributes=1 winding=1 blend_order=1 no_triangle_filter=1\n",checks);
    return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"map_mesh_optimization=failed %s\n",e.what());return 1;}
}
