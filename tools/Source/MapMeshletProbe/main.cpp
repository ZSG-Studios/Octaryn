#include "MapMeshlets.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
using namespace octaryn::client::rendering;
using Triangle=std::array<unsigned,4>;
Triangle canonical(unsigned material,unsigned a,unsigned b,unsigned c) {
  if(b<a && b<c)return {material,b,c,a};
  if(c<a && c<b)return {material,c,a,b};
  return {material,a,b,c};
}
int main() {
  MapModel model;
  for(unsigned material=0;material<3;++material) {
    MapPrimitive primitive;primitive.first_index=unsigned(model.indices.size());
    const unsigned base=unsigned(model.vertices.size());
    for(unsigned z=0;z<=32;++z)for(unsigned x=0;x<=32;++x) {
      MapVertex vertex{};vertex.position[0]=float(x)+float(material)*40;
      vertex.position[1]=std::sin(float(x)*.2f)*.1f;vertex.position[2]=float(z);
      vertex.normal[1]=1;vertex.uv[0]=float(x)/32;vertex.uv[1]=float(z)/32;
      model.vertices.push_back(vertex);
    }
    for(unsigned z=0;z<32;++z)for(unsigned x=0;x<32;++x) {
      const unsigned a=base+z*33+x,b=a+1,c=a+33,d=c+1;
      model.indices.insert(model.indices.end(),{a,c,b,b,c,d});
    }
    primitive.index_count=unsigned(model.indices.size())-primitive.first_index;
    primitive.material.alpha_mode=material==0?MapAlphaMode::Opaque:material==1?MapAlphaMode::Mask:MapAlphaMode::Blend;
    model.primitives.push_back(primitive);
  }
  MapMeshletData data;std::string error;
  if(!prepare_map_meshlets(model,data,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 1;}
  std::map<Triangle,unsigned> expected,actual;
  for(unsigned material=0;material<2;++material) {
    const auto& p=model.primitives[material];
    for(unsigned i=0;i<p.index_count;i+=3) {
      const auto* triangle=model.indices.data()+p.first_index+i;
      ++expected[canonical(material,triangle[0],triangle[1],triangle[2])];
    }
  }
  for(const auto& mesh:data.records) {
    if(!mesh.vertex_count || mesh.vertex_count>128 || !mesh.triangle_count || mesh.triangle_count>256 || mesh.material>1)return 2;
    for(unsigned i=0;i<mesh.vertex_count;++i) {
      const unsigned vertex=data.vertices.at(mesh.vertex_offset+i);
      const auto& v=model.vertices.at(vertex);float squared=0;
      for(unsigned axis=0;axis<3;++axis) {const float d=v.position[axis]-mesh.sphere[axis];squared+=d*d;}
      if(std::sqrt(squared)>mesh.sphere[3])return 3;
    }
    for(unsigned i=0;i<mesh.triangle_count;++i) {
      const auto packed=data.triangles.at(mesh.triangle_offset+i);
      const unsigned a=packed&255,b=(packed>>8)&255,c=(packed>>16)&255;
      if(a>=mesh.vertex_count || b>=mesh.vertex_count || c>=mesh.vertex_count)return 4;
      ++actual[canonical(mesh.material,data.vertices.at(mesh.vertex_offset+a),
          data.vertices.at(mesh.vertex_offset+b),data.vertices.at(mesh.vertex_offset+c))];
    }
  }
  if(actual!=expected)return 5;
  std::printf("meshlet_probe passed=1 meshlets=%zu triangles=%zu material_boundaries=preserved winding=preserved bounds=conservative\n",
      data.records.size(),data.triangles.size());
  return 0;
}
