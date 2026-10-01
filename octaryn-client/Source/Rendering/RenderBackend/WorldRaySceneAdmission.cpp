#include "WorldRayTracingState.h"
#include "WorldRaySceneAdmission.h"
#include "../../MapWorld/MapRendererInternal.h"
#include <slang-rhi/acceleration-structure-utils.h>

namespace octaryn::client::rendering {
namespace world_ray {
struct SceneAdmission {
  std::shared_ptr<virtual_geometry::SceneMemoryLease> credit;
  std::vector<std::shared_ptr<MapRenderer>> maps;
  std::vector<std::shared_ptr<const virtual_geometry::RaySnapshot>> rays;
  std::vector<std::uint64_t> revisions;
  std::uint64_t items{};
};
}
SceneRayAdmission world_ray_admit_scene(WorldRenderer& r,std::span<const std::shared_ptr<MapRenderer>> maps,
    const MapRenderer* replacement_owner,std::shared_ptr<const virtual_geometry::RaySnapshot> replacement) {
  if(!r.scene_memory || !r.ray_requested || !world_ray_available(r))return SceneRayAdmission::Ready;
  auto& s=*r.ray_tracing->state;auto plan=std::make_shared<world_ray::SceneAdmission>();
  plan->maps.assign(maps.begin(),maps.end());plan->items=r.items.revision;
  std::uint64_t instances=s.columns.size()+r.items.instances.size(),map_records=r.items.assets.size();
  rhi::IBuffer* address{};
  for(const auto& map:maps) {
    const auto ray=map.get()==replacement_owner?replacement:map->geometry_ray?map->geometry_ray->snapshot():nullptr;
    if(!ray || ray->blas.empty())return SceneRayAdmission::Deferred;
    plan->rays.push_back(ray);plan->revisions.push_back(map->geometry_instances_revision);
    const auto count=ray->blas.size()*std::max<std::size_t>(1,map->geometry_instances.size());
    instances+=count;map_records+=count;address=ray->vertices;
  }
  if(s.scene_admission && s.scene_admission->maps==plan->maps && s.scene_admission->rays==plan->rays &&
      s.scene_admission->revisions==plan->revisions && s.scene_admission->items==plan->items)return SceneRayAdmission::Ready;
  if(instances>=0xFFFFFFu || map_records>=0x800000u) {
    r.status="scene TLAS instance capacity exceeded";return SceneRayAdmission::Failed;
  }
  for(const auto& frame:s.frames)if(frame.instances)address=frame.instances;
  if(!address) {r.status="scene TLAS size-query address unavailable";return SceneRayAdmission::Failed;}
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(rhi::getAccelerationStructureInstanceDescType(r.device));
  if(!stride) {r.status="scene TLAS instance layout unavailable";return SceneRayAdmission::Failed;}
  rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
  input.instances.instanceBuffer=address;input.instances.instanceStride=static_cast<unsigned>(stride);
  input.instances.instanceCount=static_cast<unsigned>(std::max<std::uint64_t>(1,instances));
  rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;rhi::AccelerationStructureSizes sizes{};
  if(SLANG_FAILED(r.device->getAccelerationStructureSizes(build,&sizes)) || !sizes.accelerationStructureSize) {
    r.status="scene TLAS complete-candidate size query failed";return SceneRayAdmission::Failed;
  }
  const auto rounded=[](std::uint64_t bytes,std::uint64_t unit) {return std::bit_ceil((std::max(bytes,unit)+unit-1)/unit)*unit;};
  std::uint64_t material=rounded(map_records*sizeof(MapRayGeometry),sizeof(MapRayGeometry));
  std::uint64_t records=rounded(s.columns.size()*sizeof(Record),sizeof(Record));
  std::uint64_t tlas=std::bit_ceil(sizes.accelerationStructureSize);
  std::uint64_t native=rounded(std::max<std::uint64_t>(1,instances)*stride,stride);
  std::uint64_t scratch=rounded(std::max(sizes.scratchSize,sizes.updateScratchSize),4),dummy{};
  const auto fits=[&](const std::shared_ptr<Snapshot>& scene) {
    return scene && scene->map_records && scene->map_records->getDesc().size>=material &&
        scene->records && scene->records->getDesc().size>=records && scene->tlas && scene->tlas->getDesc().size>=tlas;
  };
  // Capacity already charged by adoption needs no duplicate reservation when
  // every possible frame/snapshot slot can perform the complete rebuild in place.
  const bool scene_growth=s.prewarm_items?
      std::any_of(s.snapshot_pool.begin(),s.snapshot_pool.end(),[&](const auto& scene){return !fits(scene);}):!fits(s.spare);
  const bool frame_growth=std::any_of(s.frames.begin(),s.frames.end(),[&](const auto& frame) {
    return !frame.instances || frame.instances->getDesc().size<native || !frame.scratch || frame.scratch->getDesc().size<scratch;
  });
  const auto capacity=[&](const std::shared_ptr<Snapshot>& scene) {
    if(!scene)return;
    if(scene->map_records)material=std::max(material,scene->map_records->getDesc().size);
    if(scene->records)records=std::max(records,scene->records->getDesc().size);
    if(scene->tlas)tlas=std::max(tlas,scene->tlas->getDesc().size);
  };
  capacity(s.current);capacity(s.spare);for(const auto& scene:s.snapshot_pool)capacity(scene);
  for(const auto& frame:s.frames) {
    capacity(frame.snapshot);capacity(frame.update_source);
    if(frame.instances)native=std::max(native,frame.instances->getDesc().size);
    if(frame.scratch)scratch=std::max(scratch,frame.scratch->getDesc().size);
    const auto retained=(frame.dummy_bounds?frame.dummy_bounds->getDesc().size:0)+
        (frame.dummy_scratch?frame.dummy_scratch->getDesc().size:0);
    dummy=std::max(dummy,retained);
  }
  // The old membership remains complete if the full next allocation cannot fit.
  // Replacing unused admission credit is safe; it owns no GPU resources.
  s.scene_admission.reset();
  plan->credit=r.scene_memory->reserve((scene_growth?material+records+tlas:0)+(frame_growth?native+scratch+dummy:0),
      virtual_geometry::SceneMemoryDomain::WorldRay,virtual_geometry::SceneMemoryPhase::Pending);
  if(!plan->credit)return SceneRayAdmission::Deferred;
  s.scene_admission=std::move(plan);return SceneRayAdmission::Ready;
}
std::shared_ptr<virtual_geometry::SceneMemoryLease> world_ray_scene_credit(WorldRenderer& r,std::uint64_t bytes) {
  const auto& plan=r.ray_tracing->state->scene_admission;
  return plan?plan->credit->split(bytes,virtual_geometry::SceneMemoryPhase::Pending):nullptr;
}
void world_ray_scene_admission_complete(WorldRenderer& r) {r.ray_tracing->state->scene_admission.reset();}
}
