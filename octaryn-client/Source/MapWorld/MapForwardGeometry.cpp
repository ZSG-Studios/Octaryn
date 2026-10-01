#include "MapForwardGeometry.h"
#include <algorithm>
#include <limits>

namespace octaryn::client::rendering {
bool build_map_forward_geometry(const MapModel& model,MapForwardGeometry& output,std::string& error,std::uint64_t byte_budget) {
  MapForwardGeometry next;next.first_indices.resize(model.primitives.size(),UINT32_MAX);
  std::uint64_t blend_indices{};
  for(const auto& primitive:model.primitives)if(primitive.material.alpha_mode==MapAlphaMode::Blend)blend_indices+=primitive.index_count;
  if(!blend_indices) {output=std::move(next);error.clear();return true;}
  if(blend_indices>UINT32_MAX || blend_indices*8>byte_budget || model.vertices.size()>UINT32_MAX) {
    error="forward geometry exceeds buffer budget";return false;
  }
  std::vector<std::uint32_t> remap(model.vertices.size(),UINT32_MAX);
  next.indices.reserve(std::size_t(blend_indices));
  next.vertices.reserve(std::size_t(std::min<std::uint64_t>({model.vertices.size(),blend_indices,byte_budget/sizeof(MapVertex)})));
  for(std::size_t id=0;id<model.primitives.size();++id) {
    const auto& primitive=model.primitives[id];if(primitive.material.alpha_mode!=MapAlphaMode::Blend)continue;
    if(std::uint64_t(primitive.first_index)+primitive.index_count>model.indices.size()) {error="forward primitive range invalid";return false;}
    next.first_indices[id]=std::uint32_t(next.indices.size());
    for(std::uint32_t i=0;i<primitive.index_count;++i) {
      const auto source=model.indices[primitive.first_index+i];
      if(source>=model.vertices.size()) {error="forward vertex index invalid";return false;}
      auto& destination=remap[source];
      if(destination==UINT32_MAX) {
        if((next.vertices.size()+1)*sizeof(MapVertex)+blend_indices*8>byte_budget) {error="forward geometry exceeds buffer budget";return false;}
        destination=std::uint32_t(next.vertices.size());next.vertices.push_back(model.vertices[source]);
      }
      next.indices.push_back(destination);
    }
  }
  output=std::move(next);error.clear();return true;
}
}
