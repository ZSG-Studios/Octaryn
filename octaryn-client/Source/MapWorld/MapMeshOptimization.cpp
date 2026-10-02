#include "MapMeshOptimization.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <limits>

namespace octaryn::client::rendering {
bool optimize_map_mesh(MapModel& model,std::string& error) {
  try {
    const auto input_vertices=model.vertices.size();
    constexpr auto unused=std::numeric_limits<std::uint32_t>::max();
    std::vector<MapVertex> vertices;
    vertices.reserve(input_vertices);
    std::vector<std::uint32_t> indices(model.indices.size()),lookup(input_vertices,unused);
    std::vector<MapVertex> local,unique;
    std::vector<std::uint32_t> local_indices,touched,remap;
    std::size_t expected=0;
    for(const auto& primitive:model.primitives) {
      if(primitive.first_index!=expected || primitive.index_count%3 ||
          primitive.index_count>model.indices.size()-expected) {
        error="map primitive ranges must partition triangle indices";return false;
      }
      expected+=primitive.index_count;
      local.clear();local_indices.clear();touched.clear();
      for(std::size_t i=primitive.first_index;i<expected;++i) {
        const auto index=model.indices[i];
        if(index>=input_vertices) {error="map index is outside vertex buffer";return false;}
        if(lookup[index]==unused) {
          lookup[index]=static_cast<std::uint32_t>(local.size());
          local.push_back(model.vertices[index]);touched.push_back(index);
        }
        local_indices.push_back(lookup[index]);
      }
      for(auto index:touched)lookup[index]=unused;
      if(local_indices.empty())continue;
      remap.resize(local.size());
      const auto count=meshopt_generateVertexRemap(remap.data(),local_indices.data(),local_indices.size(),
          local.data(),local.size(),sizeof(MapVertex));
      unique.resize(count);
      meshopt_remapVertexBuffer(unique.data(),local.data(),local.size(),sizeof(MapVertex),remap.data());
      meshopt_remapIndexBuffer(local_indices.data(),local_indices.data(),local_indices.size(),remap.data());
      // Alpha blending is order-dependent: only opaque/cutout triangles reorder.
      if(primitive.material.alpha_mode!=MapAlphaMode::Blend)
        meshopt_optimizeVertexCache(local_indices.data(),local_indices.data(),local_indices.size(),count);
      meshopt_optimizeVertexFetch(unique.data(),local_indices.data(),local_indices.size(),unique.data(),count,sizeof(MapVertex));
      if(vertices.size()+count>unused) {error="optimized map exceeds uint32 index range";return false;}
      const auto base=static_cast<std::uint32_t>(vertices.size());
      for(std::size_t i=0;i<local_indices.size();++i)indices[primitive.first_index+i]=base+local_indices[i];
      vertices.insert(vertices.end(),unique.begin(),unique.end());
    }
    if(expected!=model.indices.size()) {error="map has indices outside primitive ranges";return false;}
    model.vertices.swap(vertices);model.indices.swap(indices);
    model.collision_indices.clear();
    for(const auto& primitive:model.primitives)if(primitive.collision)
      model.collision_indices.insert(model.collision_indices.end(),model.indices.begin()+primitive.first_index,
          model.indices.begin()+primitive.first_index+primitive.index_count);
    std::printf("map_mesh_optimized vertices_before=%zu vertices_after=%zu triangles=%zu primitives=%zu exact=1 indexed=1\n",
        input_vertices,model.vertices.size(),model.indices.size()/3,model.primitives.size());
    return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
}
