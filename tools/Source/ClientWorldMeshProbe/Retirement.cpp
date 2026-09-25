#include "Probe.h"
#include "ResourceProbePacing.h"
#include "RetirementDrain.h"
#include "WorldMeshJob.h"
#include "WorldRayTracingState.h"
#include <algorithm>
#include <cstdio>
#include <map>

namespace mesh_probe {
namespace {
constexpr std::size_t Columns=65;
constexpr std::uint32_t Batch=32;
using RayColumn=octaryn::client::rendering::world_ray::Column;
using RaySnapshot=octaryn::client::rendering::world_ray::Snapshot;
struct Owners {
  std::uint64_t allocations{};
  std::array<std::size_t,7> maps{};
  std::array<std::size_t,2> batch{};
  std::map<const RaySnapshot*,std::size_t> snapshots;
  std::uint64_t remaining() const {
    std::uint64_t result=allocations;
    for(std::size_t i=0;i<maps.size();++i)if(i!=5)result+=maps[i];
    for(const auto count:batch)result+=count;
    for(const auto& [snapshot,count]:snapshots)result+=count;
    return result;
  }
};
Owners owners(const WorldRenderer& r) {
  Owners result;
  if(r.mesh_allocator)result.allocations=r.mesh_allocator->pending()+(r.mesh_allocator->finished()?0u:1u);
  result.maps={r.columns.size(),r.sources.size(),r.prediction_bases.size()};
  result.maps[6]=r.block_lights.columns.size();
  if(r.batch)for(std::size_t i=0;i<result.batch.size();++i)result.batch[i]=r.batch->frames[i].retained.size();
  if(r.ray_tracing) {
    const auto& ray=*r.ray_tracing->state;
    result.maps[3]=ray.columns.size();result.maps[4]=ray.changed.size();
    result.maps[5]=ray.built_pass_counts.size();
    const auto add=[&](const std::shared_ptr<RaySnapshot>& snapshot) {
      if(snapshot)result.snapshots[snapshot.get()]=snapshot->columns.size();
    };
    add(ray.current);
    for(const auto& frame:ray.frames) {add(frame.snapshot);add(frame.update_source);}
  }
  return result;
}
std::vector<std::weak_ptr<RayColumn>> populate(Fixture& fixture) {
  auto& r=fixture.renderer;
  std::uint16_t stone{};
  for(std::size_t i=0;i<fixture.catalog.size();++i)
    if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=static_cast<std::uint16_t>(i);
  require(stone!=0,"retirement fixture stone material missing");
  auto source=column(0,0,0,1);put(source,4,0,4,stone);source.blocks.compact();
  const auto primary=fixture.mesh(source);
  auto old_source=source;old_source.x=1;old_source.revision=2;
  const auto old=fixture.mesh(old_source);
  r.sources.clear();r.radius=32;r.center_x=32;r.center_z=0;
  r.batch=std::make_unique<WorldBatch>();r.batch->device=r.device;
  r.ray_tracing=std::make_unique<WorldRayTracing>();auto& ray=*r.ray_tracing->state;
  auto snapshot=std::make_shared<RaySnapshot>();
  std::vector<std::weak_ptr<RayColumn>> watched;
  for(std::size_t i=0;i<Columns;++i) {
    const auto coordinate=std::make_pair(static_cast<int>(i),0);
    auto resident=source;resident.x=coordinate.first;
    r.sources.emplace(coordinate,resident);r.prediction_bases.emplace(coordinate,resident);
    r.columns.emplace(coordinate,primary.gpu);
    r.block_lights.columns[coordinate].blocks=resident.blocks;
    auto current=std::make_shared<RayColumn>();current->faces=primary.gpu.faces;current->fluids=primary.gpu.fluids;
    ray.columns.emplace(coordinate,current);ray.changed.emplace(coordinate,current);
    ray.built_pass_counts.emplace(coordinate,primary.gpu.pass_counts);
    const auto old_coordinate=std::make_pair(static_cast<int>(i)+128,0);
    auto ray_only=std::make_shared<RayColumn>();ray_only->faces=old.gpu.faces;ray_only->fluids=old.gpu.fluids;
    ray.columns.emplace(old_coordinate,ray_only);ray.changed.emplace(old_coordinate,ray_only);
    ray.built_pass_counts.emplace(old_coordinate,old.gpu.pass_counts);
    auto retired=std::make_shared<RayColumn>();retired->faces=old.gpu.faces;retired->fluids=old.gpu.fluids;
    snapshot->columns.push_back(retired);watched.emplace_back(retired);
    for(auto& frame:r.batch->frames)for(unsigned pass=0;pass<2;++pass) {
      frame.retained.push_back(primary.gpu.faces);frame.retained.push_back(primary.gpu.patches);
    }
  }
  r.block_lights.columns[{256,0}].blocks=source.blocks;
  // Alias the same old-only snapshot through all five scene owners. The batch
  // limit must apply once to its vector, not once for every shared_ptr alias.
  ray.current=snapshot;
  ray.spare=std::make_shared<RaySnapshot>();ray.spare->records=primary.gpu.faces;
  for(auto& frame:ray.frames) {frame.snapshot=snapshot;frame.update_source=snapshot;}
  ray.jobs[0].pending=ray.columns.begin()->second;ray.jobs[0].refit_source=snapshot->columns.front();
  r.draw_list.visible.push_back({&r.columns.begin()->second,0});
  r.dirty.insert({32,0});r.dirty_urgent.insert({32,0});
  r.qualification_mesh=std::make_unique<WorldMeshJob>();
  require(r.qualification_mesh->start(r,source),"retirement pending qualification count submission failed");
  require(world_mesh_refresh_one(r) && r.halo_jobs && r.halo_jobs->pending()!=0,
      "retirement pending halo count submission failed");
  r.delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  return watched;
}
}
void retirement_cases(Fixture& fixture) {
  auto& r=fixture.renderer;
  const auto watched=populate(fixture);
  const std::weak_ptr<RaySnapshot> spare=r.ray_tracing->state->spare;
  resource_probe::start();
  const auto initial=owners(r);
  const auto pending=open_world_renderer_retire_step(&r,Batch);
  require(pending==initial.remaining() && pending>0 && owners(r).maps==initial.maps,
      "retirement step freed owners before queue synchronization or omitted retained owners");
  require(open_world_renderer_begin_retirement(&r),"retirement initial queue synchronization failed");
  require(open_world_renderer_begin_retirement(&r),"retirement begin must be idempotent");
  require(spare.expired() && !r.ray_tracing->state->spare,"retirement retained spare ray capacity owner");
  if(r.mesh_allocator)require(r.mesh_allocator->finish(1000000000ull),"retirement fixture idle allocator stop");
  require(!r.delivery_jobs && !r.halo_jobs && !r.qualification_mesh && r.draw_list.visible.empty(),
      "retirement retained bounded jobs or dangling draw pointers after synchronization");
  const auto paused=owners(r);
  require(open_world_renderer_retire_step(&r,Batch,0.0)==paused.remaining() &&
      owners(r).maps==paused.maps && owners(r).batch==paused.batch && owners(r).snapshots==paused.snapshots,
      "zero retirement admission budget released ownership");
  std::uint64_t previous=pending;
  unsigned steps{},admitted{};
  for(;;) {
    resource_probe::Frame frame_scope("retirement_maintenance");
    const auto before=owners(r);
    const auto batch=steps==0?1u:Batch;
    // Explicit qualification exercises both single-unit and maximum batches.
    const auto remaining=open_world_renderer_retire_step(&r,batch,1000.0);
    const auto after=owners(r);
    for(std::size_t i=0;i<before.maps.size();++i)
      require(after.maps[i]<=before.maps[i] && before.maps[i]-after.maps[i]<=batch,
          "retirement exceeded per-owner map batch or added work");
    for(std::size_t i=0;i<before.batch.size();++i)
      require(after.batch[i]<=before.batch[i] && before.batch[i]-after.batch[i]<=batch*4,
          "retirement exceeded retained draw-reference batch");
    for(const auto& [snapshot,count]:before.snapshots) {
      const auto found=after.snapshots.find(snapshot);
      const auto next=found==after.snapshots.end()?0:found->second;
      require(next<=count && count-next<=batch,"aliased or old-only snapshot bypassed retirement batch bound");
    }
    require(remaining==after.remaining() || (after.remaining()==0 && remaining==1),
        "retirement count omitted world owners or exceeded one backend drain unit");
    require(after.remaining()<previous || after.remaining()==0,"retirement failed to make bounded owner progress");
    const auto alive=static_cast<std::size_t>(std::count_if(watched.begin(),watched.end(),[](const auto& value){return !value.expired();}));
    admitted+=batch;
    require(alive==std::max<std::size_t>(Columns-std::min<std::size_t>(Columns,admitted),0),
        "old-only ray columns were freed early or retained after their bounded batch");
    const auto frame=r.frames;
    require(open_world_renderer_retirement_frame(&r) && r.frames==frame+1,
        "production headless retirement frame did not complete a real GPU submission");
    ++steps;require(steps<=8,"bounded retirement failed to converge");
    previous=after.remaining();if(previous==0)break;
  }
  require(steps>=5,"retirement fixture did not exercise ray-only and lighting-only coordinates");
  require(r.ray_tracing && !r.ray_tracing->state->jobs[0].pending && !r.ray_tracing->state->jobs[0].refit_source,
      "retirement retained ray build aliases");
  drain_retirement(r);
  require(open_world_renderer_retire_step(&r,Batch)==0,"completed retirement restarted work");
  const auto final=owners(r);
  require(std::all_of(final.maps.begin(),final.maps.end(),[](auto count){return count==0;}) &&
      std::all_of(final.batch.begin(),final.batch.end(),[](auto count){return count==0;}) &&
      final.snapshots.empty(),"retirement reported completion with retained owners");
  require(r.device && r.queue && r.atlas && r.mesh_pipeline,"retirement destroyed shared renderer infrastructure early");
  require(r.debug.errors.load()==0,"retirement graphics validation errors");
  std::printf("world_retirement=passed logical_columns=%zu ray_only_columns=%zu gpu_output_sets=2 ray_ownership=synthetic pending_gpu_jobs=2 batches=%u snapshot_aliases=5 old_only_lifetime=passed headless_maintenance=production zero_budget=1 single_unit=1 spare_capacity_released=1 validation_errors=0\n",Columns,Columns,steps);
}
}
