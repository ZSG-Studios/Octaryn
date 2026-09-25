#include "WorldRendererInternal.h"
#include "WorldMeshInput.h"

namespace octaryn::client::rendering {
namespace {
bool boundary_changed(const world_presentation::StreamColumn* previous,
    const world_presentation::StreamColumn& next,int dx,int dz) {
  // New residency or a different vertical extent may alter any neighbor input.
  if(!previous || previous->min_y!=next.min_y || previous->height!=next.height ||
      next.height<=0 || next.height>world_presentation::StreamWorldHeight) return true;
  const auto size=static_cast<std::size_t>(next.height)*32u*32u;
  if(previous->blocks.size()!=size || next.blocks.size()!=size) return true;
  const int first_x=dx>0?31:0,last_x=dx==0?31:first_x;
  const int first_z=dz>0?31:0,last_z=dz==0?31:first_z;
  // Each neighbor consumes exactly its facing edge (or corner) at every Y.
  // Compare voxel data, never revision hashes, so all fluid level/flow inputs count.
  for(int z=first_z;z<=last_z;++z) for(int y=0;y<next.height;++y)
    for(int x=first_x;x<=last_x;++x) {
      const auto index=static_cast<std::size_t>(x)+32u*(static_cast<std::size_t>(y)+
          static_cast<std::size_t>(next.height)*static_cast<std::size_t>(z));
      if(previous->blocks[index]!=next.blocks[index]) return true;
    }
  return false;
}
}

void world_mesh_invalidate_neighbors(WorldRenderer& renderer,
    const world_presentation::StreamColumn& next) {
  const auto old=renderer.sources.find({next.x,next.z});
  const auto* previous=old==renderer.sources.end()?nullptr:&old->second;
  for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
    if(dx==0 && dz==0) continue;
    const auto coordinate=std::make_pair(next.x+dx,next.z+dz);
    if(!renderer.sources.contains(coordinate)) continue;
    if(!previous) {
      const auto mesh=renderer.columns.find(coordinate);
      if(mesh!=renderer.columns.end() && world_mesh_preload_matches(
          mesh->second.preloaded_neighbors[world_presentation::column_neighbor_index(-dx,-dz)],next))continue;
    }
    if(renderer.dirty.contains(coordinate) && (!previous || renderer.dirty_urgent.contains(coordinate))) continue;
    if(boundary_changed(previous,next,dx,dz)) {
      renderer.dirty.insert(coordinate);
      // An edit must promote work already queued by an earlier column arrival.
      if(previous) renderer.dirty_urgent.insert(coordinate);
    }
  }
}
void world_mesh_invalidate_preloaded_outside_window(WorldRenderer& renderer) {
  for(const auto& [coordinate,column]:renderer.columns) {
    if(renderer.dirty_urgent.contains(coordinate))continue;
    for(std::size_t index=0;index<column.preloaded_neighbors.size();++index) {
      if(!column.preloaded_neighbors[index])continue;
      const int offset=int(index)+(index>=4?1:0),dx=offset%3-1,dz=offset/3-1;
      if(std::abs(std::int64_t(coordinate.first)+dx-renderer.center_x)<=renderer.radius &&
          std::abs(std::int64_t(coordinate.second)+dz-renderer.center_z)<=renderer.radius)continue;
      // The absent neighbor may never have published, so eviction alone cannot
      // discover that this mesh must restore its newly exposed boundary.
      renderer.dirty.insert(coordinate);renderer.dirty_urgent.insert(coordinate);
      break;
    }
  }
}
}
