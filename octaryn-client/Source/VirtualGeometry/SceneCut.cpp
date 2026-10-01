#include "SceneSessionInternal.h"
#include "WorldGeometry.h"
#include "SceneMemoryLedger.h"
#include "SceneRasterTables.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace octaryn::client::rendering {
bool SceneSession::State::update_cut(WorldRenderer& renderer,const WorldCamera& camera) {
  if(!startup_committed && pending.empty()) {
    for(const auto root:assets.roots())if(!assets.selection(root).instances.empty())pending.push_back(root);
    if(pending.empty()) {error="scene has no instantiated geometry roots";return false;}
  }
  if(startup_committed && pending.empty() && !job.task && assets.hierarchical() && !cut.empty()) {
    const float focal=.5f*renderer.render_height()/std::tan(camera.vertical_fov*.5f);
    // Spread hierarchy evaluation across frames. A candidate never replaces only some children.
    for(unsigned inspected=0;inspected<std::min<std::size_t>(32,cut.size());++inspected) {
      const auto parent=cut[(scan++)%cut.size()];const auto& render=assets.render_part(parent);
      if(render.parent!=virtual_geometry::invalid_id && retry_after[render.parent]<=frame) {
        const auto& ancestor=assets.render_part(render.parent);
        const bool siblings=std::all_of(ancestor.children.begin(),ancestor.children.end(),[&](auto child) {
          return std::find(cut.begin(),cut.end(),child)!=cut.end();
        });
        if(siblings && assets.projected_error(render.parent,camera,focal)<pixel_error*.7f) {
          replacing=render.parent;removed=ancestor.children;pending={render.parent};break;
        }
      }
      if(render.exact || retry_after[parent]>frame || assets.projected_error(parent,camera,focal)<=pixel_error)continue;
      replacing=parent;removed={parent};
      if(render.expanded)pending=render.children;
      else {
        job.part=parent;job.assets=&assets;job.expand=true;job.exact=render.node.children.empty();
        job.cancelled=false;job.success=false;job.error.clear();
        octaryn_native_schedule_runtime_job description{};description.job_id="scene_hierarchy_refine";
        description.context=&job;description.execute=Job::execute;
        job.task=octaryn_native_schedule_runtime_submit_worker(scheduler,&description,1);
        if(!job.task) {error="scene hierarchy refinement submission failed";return false;}
      }
      break;
    }
  }
  plan={};plan.admitted=true;
  for(const auto id:startup_committed?cut:pending)plan.wanted.push_back(assets.selection(id));
  plan.reservation_bytes=assets.context().ledger->stats().used;
  return true;
}
bool SceneSession::State::commit_cut(WorldRenderer& renderer) {
  if(pending.empty())return true;
  for(const auto id:pending) {
    const auto found=entries.find(id);if(found==entries.end() || !found->second.ready)return true;
  }
  std::vector<std::uint32_t> next=cut;
  for(const auto id:removed)std::erase(next,id);
  next.insert(next.end(),pending.begin(),pending.end());std::sort(next.begin(),next.end());
  if(std::adjacent_find(next.begin(),next.end())!=next.end()) {error="scene hierarchy cut contains duplicate owners";return false;}
  if(replacing!=virtual_geometry::invalid_id) {
    std::uint64_t coverage{},original{};
    for(const auto id:pending)coverage+=assets.parts()[id].triangle_count;
    for(const auto id:removed)original+=assets.parts()[id].triangle_count;
    if(coverage!=original) {error="scene hierarchy replacement loses source coverage";return false;}
  }
  std::vector<std::shared_ptr<MapRenderer>> candidate;candidate.reserve(next.size());
  for(const auto id:next)candidate.push_back(entries.at(id).map);
  virtual_geometry::SceneRasterCapacity capacity;
  for(const auto& map:candidate) {
    const auto& geometry=map->geometry->asset();capacity.clusters+=geometry.clusters.size();capacity.pages+=geometry.pages.size();
    capacity.instances+=map->geometry_instances.size();capacity.draws+=geometry.clusters.size()*map->geometry_instances.size();
  }
  const auto defer=[&] {
    retry_after[replacing]=frame+120;
    for(const auto id:pending)if(auto entry=entries.find(id);entry!=entries.end())retire(entry);
    pending.clear();removed.clear();replacing=virtual_geometry::invalid_id;
  };
  if(!assets.context().raster->reserve(capacity)) {
    if(startup_committed && assets.context().raster->admission_rejected()) {defer();return true;}
    error=assets.context().raster->error();return false;
  }
  const auto admission=world_ray_admit_scene(renderer,candidate);
  if(admission==SceneRayAdmission::Failed) {error=renderer.status;return false;}
  if(admission==SceneRayAdmission::Deferred) {
    if(startup_committed) {defer();return true;}
    error="complete coarse scene TLAS exceeds aggregate GPU budget";return false;
  }
  for(const auto id:pending)entries.at(id).published=true;
  for(const auto id:removed)retire(entries.find(id));
  cut=std::move(next);pending.clear();removed.clear();replacing=virtual_geometry::invalid_id;startup_committed=true;changed=true;
  plan.wanted.clear();for(const auto id:cut)plan.wanted.push_back(assets.selection(id));
  std::printf("scene_cut_committed frame=%llu owners=%zu original_source_complete=1 parent_or_children=1 ledger_used=%llu\n",
      static_cast<unsigned long long>(render_frame),cut.size(),static_cast<unsigned long long>(assets.context().ledger->stats().used));
  return true;
}
}
