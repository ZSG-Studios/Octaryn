#include "MapMeshlets.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
namespace octaryn::client::rendering {
bool map_meshlet_requested() {
  const char* mode=std::getenv("OCTARYN_CLIENT_MAP_DRAW_MODE");
  return mode && std::strcmp(mode,"meshlet")==0;
}
bool prepare_map_meshlets(const MapModel& model,MapMeshletData& output,std::string& error) {
  MapMeshletData data;
  if(model.vertices.empty() || model.indices.empty()) {error="meshlet source geometry empty";return false;}
  for(std::size_t material=0;material<model.primitives.size();++material) {
    const auto& primitive=model.primitives[material];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    if(primitive.first_index>model.indices.size() || primitive.index_count>model.indices.size()-primitive.first_index ||
        primitive.index_count%3) {error="meshlet primitive range invalid";return false;}
    const auto bound=meshopt_buildMeshletsBound(primitive.index_count,map_meshlet_vertices,map_meshlet_triangles);
    std::vector<meshopt_Meshlet> records(bound);
    std::vector<unsigned> vertices(bound*map_meshlet_vertices);
    std::vector<unsigned char> triangles(bound*map_meshlet_triangles*3);
    const auto count=meshopt_buildMeshlets(records.data(),vertices.data(),triangles.data(),
        model.indices.data()+primitive.first_index,primitive.index_count,model.vertices.front().position,
        model.vertices.size(),sizeof(MapVertex),map_meshlet_vertices,map_meshlet_triangles,0);
    for(std::size_t index=0;index<count;++index) {
      const auto& mesh=records[index];auto* vertex=vertices.data()+mesh.vertex_offset;
      auto* triangle=triangles.data()+mesh.triangle_offset;
      meshopt_optimizeMeshlet(vertex,triangle,mesh.triangle_count,mesh.vertex_count);
      const auto bounds=meshopt_computeMeshletBounds(vertex,triangle,mesh.triangle_count,
          model.vertices.front().position,model.vertices.size(),sizeof(MapVertex));
      if(data.vertices.size()>std::numeric_limits<unsigned>::max()-mesh.vertex_count ||
          data.triangles.size()>std::numeric_limits<unsigned>::max()-mesh.triangle_count) {
        error="meshlet payload exceeds addressable range";return false;
      }
      MapMeshlet result;result.vertex_offset=unsigned(data.vertices.size());result.triangle_offset=unsigned(data.triangles.size());
      result.vertex_count=mesh.vertex_count;result.triangle_count=mesh.triangle_count;result.material=unsigned(material);
      std::copy_n(bounds.center,3,result.sphere);result.sphere[3]=bounds.radius+std::max(1e-4f,bounds.radius*1e-5f);
      // Double-sided materials keep a degenerate cone (axis 0, cutoff 1) that never culls.
      if(!primitive.material.double_sided) {
        std::copy_n(bounds.cone_apex,3,result.cone_apex);std::copy_n(bounds.cone_axis,3,result.cone_axis);
        result.cone_cutoff=bounds.cone_cutoff;
      } else result.cone_cutoff=1;
      data.records.push_back(result);data.vertices.insert(data.vertices.end(),vertex,vertex+mesh.vertex_count);
      for(unsigned t=0;t<mesh.triangle_count;++t)
        data.triangles.push_back(unsigned(triangle[t*3])|(unsigned(triangle[t*3+1])<<8)|(unsigned(triangle[t*3+2])<<16));
    }
  }
  output=std::move(data);error.clear();return true;
}
}
