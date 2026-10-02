#include "SceneSessionInternal.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>

namespace octaryn::client::rendering {
bool SceneAssets::apply_objects(std::span<const SceneObjectPose> poses,std::string& error) {
  struct Update {std::size_t node;virtual_geometry::GeometryTransform transform;std::array<float,16> matrix;std::array<float,6> bounds;bool removed;};
  std::vector<Update> updates;
  for(const auto& pose:poses) {
    if(pose.source_name.empty() || pose.source_name.size()>1024) {error="scene object identity is invalid";return false;}
    virtual_geometry::GeometryTransform delta;
    if(!virtual_geometry::geometry_transform(pose.delta,delta,error))return false;
    const auto named=named_nodes_.find(std::string(pose.source_name));if(named==named_nodes_.end())continue;
    for(const auto node:named->second) {
      const auto& source=catalog_.instances[node];
      std::array<float,16> matrix{};
      for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row)
        for(unsigned k=0;k<4;++k)matrix[column*4+row]+=pose.delta[k*4+row]*source.transform[column*4+k];
      virtual_geometry::GeometryTransform transform;
      if(!virtual_geometry::geometry_transform(matrix,transform,error))return false;
      updates.push_back({node,transform,matrix,virtual_geometry::geometry_transform_bounds(delta,source.bounds),pose.removed});
    }
  }
  for(const auto& update:updates) {
    transforms_[update.node]=update.transform;nodes_[update.node].transform=update.matrix;
    nodes_[update.node].bounds=update.bounds;removed_nodes_[update.node]=update.removed;
    if(const char* trace=std::getenv("OCTARYN_CLIENT_SCENE_PHYSICS_TRACE_SOURCE");trace && catalog_.instances[update.node].name==trace)
      std::printf("scene_object_transform source=%s node=%zu position=%.9g,%.9g,%.9g removed=%u\n",trace,update.node,
          update.matrix[12],update.matrix[13],update.matrix[14],unsigned(update.removed));
  }
  error.clear();return true;
}
bool SceneSession::apply_objects(WorldRenderer& renderer,std::span<const SceneObjectPose> poses) {
  if(poses.empty())return true;
  auto& s=*state_;if(!s.loaded)return false;
  if(!s.assets.apply_objects(poses,s.error))return false;
  for(auto& [id,entry]:s.entries)if(entry.map)s.assets.instances(*entry.map,entry.selected.instances);
  s.changed=true;
  return s.publish(renderer);
}
std::string SceneSession::source_path() const {return state_->assets.catalog().source;}
std::string open_world_renderer_scene_source(const WorldRenderer* renderer) {
  if(!renderer || !renderer->scene_session)return {};
  return renderer->scene_physics_source.empty()?renderer->scene_session->source_path():renderer->scene_physics_source;
}
bool open_world_renderer_set_scene_objects(WorldRenderer* renderer,std::span<const SceneObjectPose> poses) {
  return renderer && renderer->scene_session && renderer->scene_session->apply_objects(*renderer,poses);
}
}
