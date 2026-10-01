#include "ScenePreparationInternal.h"
#include "ScenePreparationLock.h"
#include "SceneResidency.h"
#include "SceneBudget.h"
#include "CollisionBudget.h"
#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
using scene_geometry::Plan;
using scene_geometry::Query;
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
Plan select(const SceneCatalog& catalog,const Query& query,bool actual_budget) {
  std::vector<std::uint64_t> nodes(catalog.mesh_count);
  std::vector<scene_geometry::Instance> instances;
  for(const auto& node:catalog.instances) {++nodes[node.mesh];instances.push_back({node.node,node.mesh,node.transform,node.bounds});}
  std::vector<scene_geometry::Part> parts;
  for(const auto& part:catalog.parts) {
    const auto& primitive=catalog.primitives[part.primitive];
    const auto bytes=actual_budget?scene_part_reservation(part,primitive,nodes[primitive.mesh]):1;
    parts.push_back({primitive.mesh,primitive.primitive,part.first_triangle,part.triangle_count,bytes,
        part.bounds,!actual_budget || !part.geometry.empty(),part.bounds_prepared});
  }
  scene_geometry::ResidencyIndex index;std::string error;
  if(!index.reset(parts,instances,error))throw std::runtime_error(error);
  return index.plan(query);
}
void load(ScenePreparationWork& work,const std::filesystem::path& source={}) {
  work.check();std::string error;
  if(std::filesystem::exists(content::file_io_path(work.path))) {
    if(!read_scene_catalog(work.path,work.catalog,error))throw std::runtime_error(error);
    if(!source.empty())require(std::filesystem::equivalent(content::file_io_path(source),content::file_io_path(preparation_path(work.catalog.source))),"scene preparation catalog belongs to another source");
    work.verify_resources();
  }else {
    require(!source.empty(),"scene preparation catalog is missing");
    work.report(ScenePreparationStage::Inspect,0,1);
    if(!import_scene_catalog(source,work.catalog,error,work.cancel))throw std::runtime_error(error);
    work.verify_resources();
    work.check();work.modified=true;work.checkpoint(true);
  }
}
bool failed(ScenePreparationWork& work,ScenePreparationResult& result,std::string& error,const std::exception& failure) {
  error=failure.what();work.result.canceled=work.cancel && work.cancel->load();
  try {work.checkpoint(true);}catch(const std::exception& save) {error+="; checkpoint failed: ";error+=save.what();}
  work.finish();
  try {work.report(work.result.canceled?ScenePreparationStage::Canceled:ScenePreparationStage::Failed,0,0);}catch(...) {}
  result=work.result;return false;
}
}
bool prepare_scene_range(const std::filesystem::path& path,std::uint64_t first,std::uint64_t count,
    ScenePreparationMode mode,ScenePreparationResult& result,std::string& error,
    const std::atomic_bool* cancel,ScenePreparationNotify notify) {
  ScenePreparationWork work;work.path=path;work.cancel=cancel;work.notify=std::move(notify);
  std::unique_ptr<ScenePreparationLock> lock;
  try {
    lock=std::make_unique<ScenePreparationLock>(path);load(work);
    require(first<=work.catalog.parts.size(),"scene preparation range exceeds catalog");
    const auto size=std::min<std::uint64_t>(count,work.catalog.parts.size()-first);
    std::vector<std::uint32_t> parts(size);std::iota(parts.begin(),parts.end(),std::uint32_t(first));
    if(mode==ScenePreparationMode::Bounds)work.bounds(parts);else work.cook(parts);
    work.report(ScenePreparationStage::Ready,size,size);result=work.result;error.clear();return true;
  }catch(const std::exception& failure) {return failed(work,result,error,failure);}
}
bool prepare_scene_neighborhood(const ScenePreparationRequest& request,ScenePreparationResult& result,std::string& error,
    const std::atomic_bool* cancel,ScenePreparationNotify notify) {
  ScenePreparationWork work;work.path=request.catalog;work.cancel=cancel;work.notify=std::move(notify);
  std::unique_ptr<ScenePreparationLock> lock;
  try {
    lock=std::make_unique<ScenePreparationLock>(work.path);load(work,request.source);
    Query render;render.camera=request.camera;render.actor=request.actor;render.load_radius=render.keep_radius=request.render_radius;
    render.actor_radius=request.collision_radius;render.budget_bytes=UINT64_MAX;
    Query collision;collision.camera=collision.actor=request.actor;
    collision.load_radius=collision.keep_radius=collision.actor_radius=request.collision_radius;
    collision.budget_bytes=UINT64_MAX;
    auto visible=select(work.catalog,render,false),protected_parts=select(work.catalog,collision,false);
    require(visible.admitted || !visible.pending_parts.empty(),visible.error.c_str());
    require(protected_parts.admitted || !protected_parts.pending_parts.empty(),protected_parts.error.c_str());
    std::set<std::uint32_t> pending(visible.pending_parts.begin(),visible.pending_parts.end());
    pending.insert(protected_parts.pending_parts.begin(),protected_parts.pending_parts.end());
    work.result.pending_bounds.assign(pending.begin(),pending.end());
    const auto bound_parts=work.result.pending_bounds;
    work.bounds(bound_parts);work.bounds_reader.reset();work.result.pending_bounds.clear();
    visible=select(work.catalog,render,false);protected_parts=select(work.catalog,collision,false);
    require(visible.admitted,visible.error.c_str());require(protected_parts.admitted,protected_parts.error.c_str());
    for(const auto& selection:protected_parts.wanted) {
      work.result.collision_pairs+=selection.instances.size();
      work.result.collision_bytes+=selection.instances.size()*scene_geometry::collision_part_reservation(work.catalog.parts[selection.part].triangle_count);
    }
    work.result.render_parts=visible.wanted.size();
    for(const auto& selection:visible.wanted) {
      work.result.wanted_parts.push_back(selection.part);
      if(work.catalog.parts[selection.part].geometry.empty())work.result.pending_cooks.push_back(selection.part);
    }
    std::vector<std::uint64_t> node_counts(work.catalog.mesh_count);
    for(const auto& instance:work.catalog.instances)++node_counts[instance.mesh];
    for(const auto& selection:visible.wanted) {
      const auto& part=work.catalog.parts[selection.part];const auto& primitive=work.catalog.primitives[part.primitive];
      work.result.render_bytes+=scene_part_minimum_reservation(part,primitive,node_counts[primitive.mesh]);
    }
    require(work.result.collision_pairs<=256 && work.result.collision_bytes<=request.collision_budget_bytes,
        "complete protected scene collision neighborhood exceeds residency budget");
    require(!visible.wanted.empty() && !protected_parts.wanted.empty(),"scene neighborhood contains no renderable or collision geometry");
    require(work.result.render_bytes<=request.gpu_budget_bytes,
        "minimum complete scene neighborhood reservation exceeds render budget before cooking; finer spatial geometry is required");
    std::vector<std::uint32_t> parts;for(const auto& selection:visible.wanted)parts.push_back(selection.part);
    work.cook(parts);
    work.result.pending_cooks.clear();
    render.budget_bytes=request.gpu_budget_bytes;visible=select(work.catalog,render,true);
    work.result.render_bytes=visible.reservation_bytes;
    require(visible.admitted,visible.error.c_str());
    work.check();work.result.neighborhood_ready=true;work.report(ScenePreparationStage::Ready,parts.size(),parts.size());
    result=work.result;error.clear();return true;
  }catch(const std::exception& failure) {return failed(work,result,error,failure);}
}
}
