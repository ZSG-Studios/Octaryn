#include "SceneAssets.h"
#include "GeometryCoarse.h"
#include "SceneResourceHash.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering {
using namespace virtual_geometry;
std::uint32_t SceneAssets::append_part(std::uint32_t primitive,const SceneHierarchyNode& node) {
  SceneRenderPart render;render.primitive=primitive;render.node=node;render.hierarchy=true;
  auto& part=render.geometry;part.primitive=primitive;part.first_triangle=node.first_triangle;
  part.triangle_count=node.coarse.triangles;part.bounds=node.bounds;part.geometry=node.coarse.file;
  part.hash=node.coarse.hash;part.clusters=node.coarse.clusters;part.pages=node.coarse.pages;
  part.root_pages=unsigned(node.coarse.root_page_ids.size());part.bounds_prepared=true;
  const auto id=std::uint32_t(render_parts_.size());render_parts_.push_back(std::move(render));
  const auto& surface=catalog_.primitives[primitive];
  // Scene owners share page, selection, material and ray scratch resources.
  const auto forward=surface.surface.alpha_mode==MapAlphaMode::Blend?node.coarse.triangles*3*(sizeof(MapVertex)+8):0;
  parts_.push_back({surface.mesh,surface.primitive,node.first_triangle,node.source_triangles,
      node.coarse.metadata_bytes+forward,node.bounds,true,true});
  return id;
}
bool SceneAssets::load_render_parts(std::string& error) {
  hierarchy_path_=catalog_path_.parent_path()/"hierarchy"/"scene.json";
  if(std::filesystem::exists(hierarchy_path_)) {
    if(!read_scene_hierarchy(hierarchy_path_,hierarchy_,error))return false;
    if(!hierarchy_.complete || hierarchy_.source_hash!=catalog_.source_hash ||
        hierarchy_.unique_triangles!=catalog_.unique_triangles || hierarchy_.instances.size()!=catalog_.instances.size()) {
      error="complete scene hierarchy does not match its original catalog";return false;
    }
    for(std::size_t i=0;i<catalog_.instances.size();++i) {
      const auto& a=hierarchy_.instances[i];const auto& b=catalog_.instances[i];
      if(a.node!=b.node || a.mesh!=b.mesh || a.transform!=b.transform || a.bounds!=b.bounds) {
        error="scene hierarchy original node identity differs";return false;
      }
    }
    for(const auto& primitive:hierarchy_.primitives) {
      if(primitive.index>=catalog_.primitives.size()) {error="scene hierarchy primitive is invalid";return false;}
      const auto& original=catalog_.primitives[primitive.index];
      if(primitive.source_triangles!=original.triangles || primitive.mesh!=original.mesh || primitive.primitive!=original.primitive) {
        error="scene hierarchy original primitive identity differs";return false;
      }
      for(const auto& node:primitive.roots)roots_.push_back(append_part(primitive.index,node));
    }
  }else {
    for(const auto& part:catalog_.parts) {
      if(part.geometry.empty() || !part.bounds_prepared) {error="scene requires complete coarse hierarchy preparation";return false;}
      const auto id=std::uint32_t(render_parts_.size());SceneRenderPart render;
      render.geometry=part;render.primitive=part.primitive;render.exact=true;render_parts_.push_back(std::move(render));
      const auto& primitive=catalog_.primitives[part.primitive];
      parts_.push_back({primitive.mesh,primitive.primitive,part.first_triangle,part.triangle_count,
          std::uint64_t(part.clusters)*sizeof(GeometryCluster),part.bounds,!part.geometry.empty(),part.bounds_prepared});
      roots_.push_back(id);
    }
  }
  if(roots_.empty()) {error="scene has no complete geometry roots";return false;}
  return true;
}
scene_geometry::Selection SceneAssets::selection(std::uint32_t id) const {
  scene_geometry::Selection result;result.part=id;
  const auto mesh=catalog_.primitives[render_parts_.at(id).primitive].mesh;
  result.instances=mesh_nodes_.at(mesh);
  return result;
}
float SceneAssets::projected_error(std::uint32_t id,const WorldCamera& camera,float focal) const {
  const auto& render=render_parts_.at(id);if(!render.hierarchy || render.exact)return 0;
  float worst{};const auto nodes=selection(id).instances;
  for(const auto node:nodes) {
    const auto& transform=transforms_[node];const auto bounds=geometry_transform_bounds(transform,render.node.bounds);
    const float position[]{camera.x,camera.y,camera.z};float squared{};
    for(unsigned axis=0;axis<3;++axis) {
      const auto delta=std::max({bounds[axis]-position[axis],0.f,position[axis]-bounds[axis+3]});squared+=delta*delta;
    }
    worst=std::max(worst,render.node.coarse.error*transform.scale*focal/std::max(.001f,std::sqrt(squared)));
  }
  return worst;
}
bool SceneAssets::expand(std::uint32_t id,PreparedScenePart& result,std::string& error,const std::atomic_bool* cancel) const {
  const auto& render=render_parts_.at(id);
  if(!render.hierarchy || render.node.children.empty()) {error="scene hierarchy node has no coarse children";return false;}
  SceneHierarchyShard shard;
  if(cancel && cancel->load()) {error="scene refinement canceled";return false;}
  if(!read_scene_hierarchy_shard(hierarchy_path_,hierarchy_,render.primitive,shard,error))return false;
  std::uint64_t covered{};
  for(const auto child:render.node.children) {
    if(child>=shard.nodes.size()) {error="scene child identity is invalid";return false;}
    result.children.push_back(shard.nodes[child]);covered+=shard.nodes[child].source_triangles;
  }
  if(covered!=render.node.source_triangles) {error="scene children do not cover their complete parent";return false;}
  return true;
}
bool SceneAssets::install_children(std::uint32_t id,PreparedScenePart&& prepared,std::string& error) {
  if(render_parts_.at(id).expanded)return true;
  if(!prepared.detail.file.empty()) {
    auto node=render_parts_[id].node;node.children.clear();node.coarse=std::move(prepared.detail);
    const auto child=append_part(render_parts_[id].primitive,node);render_parts_[child].exact=true;
    render_parts_[child].parent=id;
    render_parts_[id].children={child};render_parts_[id].expanded=true;return true;
  }
  if(prepared.children.empty()) {error="scene hierarchy children are missing";return false;}
  const auto primitive=render_parts_[id].primitive;std::vector<std::uint32_t> children;
  for(const auto& child:prepared.children) {
    const auto next=append_part(primitive,child);render_parts_[next].parent=id;children.push_back(next);
  }
  render_parts_[id].children=std::move(children);render_parts_[id].expanded=true;return true;
}
bool SceneAssets::prepare_detail(std::uint32_t id,SceneHierarchyDetail& detail,PreparedScenePart& result,
    std::string& error,const std::atomic_bool*) const {
  const auto& part=render_parts_.at(id);
  if(!part.hierarchy || part.exact || !part.node.children.empty()) {error="exact detail requires a coarse source leaf";return false;}
  return detail.prepare(part.primitive,part.node.id,result.detail,error);
}
}
