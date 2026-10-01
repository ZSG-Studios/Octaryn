#include "ScenePreparationHierarchyInternal.h"
#include "ScenePreparationInternal.h"
#include "ScenePreparationLock.h"
#include "SceneResidency.h"
#include "CollisionBudget.h"
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
scene_geometry::Plan select(const ScenePreparationRequest& request,const SceneCatalog& catalog) {
  std::vector<scene_geometry::Part> parts;std::vector<scene_geometry::Instance> instances;
  for(const auto& part:catalog.parts) {
    const auto& primitive=catalog.primitives.at(part.primitive);
    parts.push_back({primitive.mesh,primitive.primitive,part.first_triangle,part.triangle_count,1,part.bounds,true,part.bounds_prepared});
  }
  for(const auto& node:catalog.instances)instances.push_back({node.node,node.mesh,node.transform,node.bounds});
  scene_geometry::ResidencyIndex index;std::string error;require(index.reset(parts,instances,error),error);
  scene_geometry::Query query;query.camera=query.actor=request.actor;
  query.load_radius=query.keep_radius=query.actor_radius=request.collision_radius;query.budget_bytes=UINT64_MAX;
  return index.plan(query);
}
}
void validate_world_collision(const ScenePreparationRequest& request,const SceneCatalog& catalog,ScenePreparationResult& result) {
  const auto plan=select(request,catalog);require(plan.admitted && plan.pending_parts.empty(),plan.error);
  result.collision_pairs=result.collision_bytes=0;
  for(const auto& selection:plan.wanted) {
    result.collision_pairs+=selection.instances.size();
    result.collision_bytes+=selection.instances.size()*scene_geometry::collision_part_reservation(catalog.parts.at(selection.part).triangle_count);
  }
  require(result.collision_pairs && result.collision_pairs<=256 && result.collision_bytes<=request.collision_budget_bytes,
      "complete protected scene collision neighborhood exceeds residency budget or contains no geometry");
}
void prepare_world_collision(const ScenePreparationRequest& request,SceneCatalog& catalog,ScenePreparationResult& result,
    const std::atomic_bool* cancel,const ScenePreparationNotify& notify) {
  const auto plan=select(request,catalog);
  require(plan.admitted || !plan.pending_parts.empty(),plan.error);
  if(!plan.pending_parts.empty()) {
    ScenePreparationLock lock(request.catalog);ScenePreparationWork work;
    work.path=request.catalog;work.catalog=catalog;work.cancel=cancel;work.notify=notify;
    work.verify_resources();work.bounds(plan.pending_parts);catalog=std::move(work.catalog);
  }
  validate_world_collision(request,catalog,result);
}
}
