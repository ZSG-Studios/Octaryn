#include "WorldRayTracingState.h"
#include <set>

namespace octaryn::client::rendering {
bool world_ray_adopt_scene_memory(WorldRenderer& r) {
  if(!r.scene_memory || !r.ray_tracing)return true;
  auto& s=*r.ray_tracing->state;
  const auto adopt=[&](std::shared_ptr<virtual_geometry::SceneMemoryLease>& lease,std::uint64_t bytes) {
    if(r.scene_memory->owns(lease)) {
      if(bytes<=lease->bytes())return true;
      r.status="world ray capacity grew without aggregate admission";return false;
    }
    auto next=r.scene_memory->reserve(bytes,virtual_geometry::SceneMemoryDomain::WorldRay);
    if(!next) {r.status="retained world ray capacities exceed scene memory budget";return false;}
    lease=std::move(next);return true;
  };
  std::set<world_ray::Snapshot*> scenes;
  if(s.current)scenes.insert(s.current.get());if(s.spare)scenes.insert(s.spare.get());
  for(const auto& scene:s.snapshot_pool)if(scene)scenes.insert(scene.get());
  for(auto& frame:s.frames) {
    if(frame.snapshot)scenes.insert(frame.snapshot.get());
    if(frame.update_source)scenes.insert(frame.update_source.get());
    std::uint64_t bytes{};
    for(auto* resource:{frame.instances.get(),frame.scratch.get(),frame.dummy_bounds.get(),frame.dummy_scratch.get()})
      if(resource)bytes+=resource->getDesc().size;
    if(!adopt(frame.scene_allocation,bytes))return false;
  }
  for(auto* scene:scenes) {
    std::uint64_t bytes=scene->tlas?scene->tlas->getDesc().size:0;
    if(scene->records)bytes+=scene->records->getDesc().size;
    if(scene->map_records)bytes+=scene->map_records->getDesc().size;
    if(!adopt(scene->scene_allocation,bytes))return false;
  }
  return adopt(s.scene_fixed_allocation,s.dummy?s.dummy->getDesc().size:0);
}
}
