#include "GeometryMesh.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
}
GeometryMesh geometry_mesh(const MapModel& model,const MapPrimitive& primitive,bool position_only,bool filter_redundant_faces) {
  require(primitive.index_count && primitive.index_count%3==0,"geometry primitive is not triangles");
  require(primitive.first_index<=model.indices.size() &&
      primitive.index_count<=model.indices.size()-primitive.first_index,"geometry primitive range invalid");
  GeometryMesh result;std::map<unsigned,unsigned> remap;
  for(size_t i=primitive.first_index;i<size_t(primitive.first_index)+primitive.index_count;++i) {
    const auto index=model.indices[i];require(index<model.vertices.size(),"geometry vertex index invalid");
    auto [entry,added]=remap.emplace(index,unsigned(result.vertices.size()));
    if(added) {
      auto vertex=model.vertices[index];
      if(position_only) {
        for(float value:vertex.uv)require(value==0,"position-only geometry contains UVs");
        for(float value:vertex.uv1)require(value==0,"position-only geometry contains UVs");
        for(float value:vertex.tangent)require(value==0,"position-only geometry contains tangents");
        for(float value:vertex.color)require(value==1,"position-only geometry contains vertex colors");
        std::fill_n(vertex.normal,3,0);
      }
      result.vertices.push_back(vertex);
    }
    result.indices.push_back(entry->second);
  }
  // Exact attribute deduplication avoids treating duplicated source vertices as
  // artificial exterior boundaries. Authored UV/normal/material seams remain.
  std::vector<unsigned> vertex_remap(result.vertices.size());
  const auto unique_count=meshopt_generateVertexRemap(vertex_remap.data(),result.indices.data(),result.indices.size(),
      result.vertices.data(),result.vertices.size(),sizeof(MapVertex));
  std::vector<MapVertex> unique(unique_count);
  meshopt_remapVertexBuffer(unique.data(),result.vertices.data(),result.vertices.size(),sizeof(MapVertex),vertex_remap.data());
  meshopt_remapIndexBuffer(result.indices.data(),result.indices.data(),result.indices.size(),vertex_remap.data());
  result.vertices=std::move(unique);
  if(filter_redundant_faces) {
    const auto count=meshopt_filterIndexBuffer(result.indices.data(),result.indices.data(),result.indices.size(),
        result.vertices.data(),result.vertices.size(),sizeof(MapVertex),sizeof(MapVertex));
    require(count!=0,"coarse primitive has no nondegenerate surface");result.indices.resize(count);
  }
  result.attributes.resize(result.vertices.size());result.locks.resize(result.vertices.size());
  for(size_t i=0;i<result.vertices.size();++i) {
    const auto& v=result.vertices[i];auto& a=result.attributes[i];
    std::copy_n(v.normal,3,a.begin());std::copy_n(v.uv,2,a.begin()+3);std::copy_n(v.uv1,2,a.begin()+5);
    std::copy_n(v.tangent,4,a.begin()+7);std::copy_n(v.color,4,a.begin()+11);
    for(float x:v.position)require(std::isfinite(x),"geometry position nonfinite");
    for(float x:a)require(std::isfinite(x),"geometry attribute nonfinite");
  }
  // Lock topological boundary vertices so separately cooked material borders stay identical.
  std::map<std::pair<unsigned,unsigned>,unsigned> edges;
  for(size_t i=0;i<result.indices.size();i+=3)for(unsigned c=0;c<3;++c) {
    const auto a=result.indices[i+c],b=result.indices[i+(c+1)%3];
    ++edges[std::minmax(a,b)];
  }
  for(const auto& [edge,count]:edges)if(count!=2) {
    result.locks[edge.first]=meshopt_SimplifyVertex_Lock;
    result.locks[edge.second]=meshopt_SimplifyVertex_Lock;
  }
  return result;
}
}
