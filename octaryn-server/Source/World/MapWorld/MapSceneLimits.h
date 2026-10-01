#pragma once
#include "MapSceneGeometry.h"
#include <fastgltf/types.hpp>
#include <algorithm>

namespace octaryn::server::map_world {
inline bool map_scene_fits(const fastgltf::Asset& asset,const MapTriangleSoup& soup) {
  if(asset.nodes.size()>100000 || asset.meshes.size()>65536 || asset.scenes.empty())return false;
  const auto scene=asset.defaultScene.value_or(0);
  if(scene>=asset.scenes.size())return false;
  std::vector<std::size_t> pending(asset.scenes[scene].nodeIndices.begin(),asset.scenes[scene].nodeIndices.end());
  std::vector<bool> visited(asset.nodes.size());
  std::uint64_t triangles=soup.triangle_count(),vertices=soup.positions.size()/3;
  while(!pending.empty()) {
    const auto index=pending.back();pending.pop_back();
    if(index>=asset.nodes.size() || visited[index])return false;
    visited[index]=true;const auto& node=asset.nodes[index];
    if(node.skinIndex)return false;
    pending.insert(pending.end(),node.children.begin(),node.children.end());
    if(!node.meshIndex)continue;
    if(*node.meshIndex>=asset.meshes.size())return false;
    for(const auto& primitive:asset.meshes[*node.meshIndex].primitives) {
      const auto position=primitive.findAttribute("POSITION");
      if(position==primitive.attributes.end() || position->accessorIndex>=asset.accessors.size())return false;
      const auto count=asset.accessors[position->accessorIndex].count;
      if(primitive.indicesAccessor && *primitive.indicesAccessor>=asset.accessors.size())return false;
      const auto indices=primitive.indicesAccessor?asset.accessors[*primitive.indicesAccessor].count:count;
      if(indices<3 || !primitive.targets.empty())return false;
      const auto added=primitive.type==fastgltf::PrimitiveType::Triangles?indices/3:indices-2;
      if(added>soup.max_triangles-triangles || count>std::uint64_t(soup.max_triangles)*3-vertices)return false;
      triangles+=added;vertices+=count;
    }
  }
  return triangles>soup.triangle_count();
}
}
