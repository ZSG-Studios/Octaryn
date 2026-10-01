#include "SceneHierarchyDetail.h"
#include "SceneHierarchyInternal.h"
#include "ScenePreparationLock.h"
#include "GeometryCook.h"
#include "GeometryCache.h"
#include "SceneOrder.h"
#include "FilePath.h"
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneHierarchyDetail::State {
  SceneHierarchyWork work;
  SceneHierarchyShard shard;
  std::uint32_t primitive{invalid_id};
};
SceneHierarchyDetail::SceneHierarchyDetail():state_(std::make_unique<State>()) {}
SceneHierarchyDetail::~SceneHierarchyDetail()=default;
bool SceneHierarchyDetail::open(const std::filesystem::path& package,std::string& error,const std::atomic_bool* cancel) {
  try {
    auto next=std::make_unique<State>();auto& work=next->work;work.request.output=package;work.source.cancel=cancel;
    if(!read_scene_hierarchy(package,work.hierarchy,error))throw std::runtime_error(error);
    const auto& h=work.hierarchy;work.source.path=preparation_path(h.catalog);work.request.catalog=work.source.path;
    work.request.target_triangles=h.target_triangles;work.request.fan_in=h.fan_in;work.request.maximum_triangles=h.maximum_triangles;
    if(!h.complete || !read_scene_catalog(work.source.path,work.source.catalog,error))throw std::runtime_error(error.empty()?"hierarchy coarse coverage is incomplete":error);
    if(hierarchy_layout_identity(work.source.catalog,work.request)!=h.identity)throw std::runtime_error("hierarchy detail source layout differs");
    work.snapshot_orders();work.source.verify_resources();work.check_snapshot();state_=std::move(next);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool SceneHierarchyDetail::prepare(std::uint32_t primitive,std::uint32_t node,SceneHierarchyGeometry& output,std::string& error) {
  try {
    auto& state=*state_;auto& work=state.work;work.check_snapshot(primitive);
    if(state.primitive!=primitive) {
      if(!read_scene_hierarchy_shard(work.request.output,work.hierarchy,primitive,state.shard,error))throw std::runtime_error(error);
      state.primitive=primitive;
    }
    const auto& leaf=state.shard.nodes.at(node);
    if(!leaf.children.empty() || leaf.leaf_part==invalid_id)throw std::runtime_error("exact detail requires a source leaf domain");
    const auto& part=work.source.catalog.parts.at(leaf.leaf_part);const auto& p=work.source.catalog.primitives.at(primitive);
    if(part.primitive!=primitive || part.first_triangle!=leaf.first_triangle || part.triangle_count!=leaf.source_triangles)
      throw std::runtime_error("hierarchy exact leaf range differs from source");
    const auto key=work.hierarchy.source_hash+":"+std::to_string(geometry_version)+":"+std::to_string(p.mesh)+":"+
        std::to_string(p.primitive)+":"+std::to_string(part.first_triangle)+":"+std::to_string(part.triangle_count)+
        (p.position_only?":flat-position":":full-attributes")+(p.triangle_order.empty()?"":":order="+p.triangle_order_hash);
    const auto hash=hierarchy_digest(key),relative="detail/"+hash+".vgeom";std::filesystem::path file;
    if(!scene_hierarchy_path(work.request.output,relative,file,error))throw std::runtime_error(error);
    ScenePreparationLock lock(file);GeometryAsset asset;
    if(!read_geometry_cache(file,hash,asset,error,true) || asset.source_triangles!=part.triangle_count || asset.space!=GeometrySpace::Object) {
      bool original=false;
      if(!part.geometry.empty() && part.hash==hash) {
        const auto old=work.source.path.parent_path()/preparation_path(part.geometry);
        original=read_geometry_cache(old,hash,asset,error,true) && asset.source_triangles==part.triangle_count && asset.space==GeometrySpace::Object;
      }
      if(!original) {
        if(!work.source.reader) {
          work.source.reader=std::make_unique<MapSourceReader>();
          if(!work.source.reader->open(preparation_path(work.hierarchy.source),work.request.output.parent_path()/"scratch",error,work.source.cancel))throw std::runtime_error(error);
        }
        MapModel model;
        if(!load_scene_part(*work.source.reader,work.source.path,work.source.catalog,part,model,error))throw std::runtime_error(error);
        work.check_snapshot(primitive);
        if(!cook_geometry(model,hash,asset,error,{p.position_only}))throw std::runtime_error(error);
        asset.space=GeometrySpace::Object;
      }
      work.check_snapshot(primitive);if(!write_geometry_cache(file,asset,error))throw std::runtime_error(error);
    }
    if(asset.material_count!=1)throw std::runtime_error("hierarchy detail material domain differs");
    work.check_snapshot(primitive);output=describe_hierarchy_geometry(relative,asset);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
