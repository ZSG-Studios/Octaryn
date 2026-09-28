#include "WorldRayTracingState.h"
#include "DeviceMemory.h"
#include "TileBudget.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <chrono>
#include <cstdio>

namespace octaryn::client::rendering {
bool world_ray_prewarm_items(WorldRenderer& r,unsigned items) {
  if(!world_ray_available(r) || !r.ray_requested || r.items.assets.empty())return true;
  auto& state=*r.ray_tracing->state;
  if(state.prewarm_items)return true;
  const auto start=std::chrono::steady_clock::now();double maximum_call_ms=0;
  const auto fail=[&](const char* step) {
    r.status="item_ray_prewarm_failed";
    std::fprintf(stderr,"world_ray_prewarm_failed step=%s\n",step);return false;
  };
  if(state.current || std::any_of(state.frames.begin(),state.frames.end(),[](const auto& frame){return bool(frame.snapshot);}))
    return fail("startup_ownership");
  if(items!=NormalItemCapacity && items!=MaximumItemCapacity)return fail("item_capacity_1000_or_10000_required");
  const auto stride=unsigned(rhi::getAccelerationStructureInstanceDescSize(rhi::getAccelerationStructureInstanceDescType(r.device)));
  if(!stride || !r.items.buffers[0])return fail("size_query_input");
  rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
  // Size queries do not read this existing buffer; no dummy scene is submitted.
  input.instances.instanceBuffer=r.items.buffers[0];input.instances.instanceStride=stride;
  input.instances.instanceCount=PrewarmMapCapacity+items+1;
  rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  rhi::AccelerationStructureSizes sizes{};
  if(SLANG_FAILED(r.device->getAccelerationStructureSizes(build,&sizes)))return fail("size_query");
  const auto plan=capacity_plan(PrewarmMapCapacity,items,unsigned(r.items.assets.size()),stride,
      sizeof(MapRayGeometry),sizeof(Record),sizes.accelerationStructureSize,std::max(sizes.scratchSize,sizes.updateScratchSize));
  if(!plan)return fail("bounded_capacity_plan");
  const auto before=device_memory_stats(r.device->getInfo(),true);
  if(before.budget_available && !tile_gpu_admits(plan->total_bytes,before.local_budget,before.local_usage,0,
      before.reserved_capacity_bytes))
    return fail("os_gpu_70_percent");
  auto reservation=std::make_shared<DeviceMemoryReservation>(plan->total_bytes);
  // Startup owns the device exclusively. Publish the pool only after every allocation succeeds.
  std::array<std::shared_ptr<Snapshot>,SceneSnapshotCount> pool;
  std::array<Slang::ComPtr<rhi::IBuffer>,SceneFrameCount> instances,scratch;
  auto timed=[&](auto&& operation) {
    const auto begin=std::chrono::steady_clock::now();const bool ok=operation();
    maximum_call_ms=std::max(maximum_call_ms,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
    return ok;
  };
  for(auto& scene:pool) {
    scene=std::make_shared<Snapshot>();
    scene->capacity_reservation=reservation;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::TopLevel;
    desc.size=plan->tlas_bytes;desc.label="world_ray_prewarmed_scene";
    if(!timed([&]{return SLANG_SUCCEEDED(r.device->createAccelerationStructure(desc,scene->tlas.writeRef()));}))
      return fail("tlas_allocation");
    if(!timed([&]{return buffer(r,plan->map_record_bytes,sizeof(MapRayGeometry),
        rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,rhi::ResourceState::ShaderResource,scene->map_records);}) ||
       !timed([&]{return buffer(r,plan->record_bytes,sizeof(Record),
        rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,rhi::ResourceState::ShaderResource,scene->records);}))
      return fail("snapshot_records");
    scene->maps.reserve(PrewarmMapCapacity);scene->map_blas.reserve(PrewarmMapCapacity);
    scene->item_assets.reserve(r.items.assets.size());
  }
  for(unsigned slot=0;slot<SceneFrameCount;++slot) {
    if(!timed([&]{return buffer(r,plan->instance_bytes,stride,
        rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
        rhi::ResourceState::AccelerationStructureBuildInput,instances[slot]);}) ||
       !timed([&]{return buffer(r,plan->scratch_bytes,4,rhi::BufferUsage::UnorderedAccess,
        rhi::ResourceState::UnorderedAccess,scratch[slot]);}))return fail("frame_capacity");
  }
  // Fresh observations do not retire the separate conservative owner-lifetime credit.
  const auto after=device_memory_stats(r.device->getInfo(),true);
  if(after.budget_available && !tile_gpu_admits(after.local_usage,after.local_budget,0,0,after.reserved_capacity_bytes))
    return fail("os_gpu_budget_changed");
  state.snapshot_records.reserve(1);state.snapshot_map_records.reserve(PrewarmMapCapacity+r.items.assets.size());
  state.snapshot_instances.reserve(plan->instances);state.snapshot_native.reserve(plan->instance_bytes);
  state.accounting_snapshots.reserve(SceneSnapshotCount+2*SceneFrameCount+2);state.accounting_columns.reserve(1);
  state.snapshot_pool=std::move(pool);
  for(unsigned slot=0;slot<SceneFrameCount;++slot) {
    state.frames[slot].instances=std::move(instances[slot]);state.frames[slot].scratch=std::move(scratch[slot]);
    state.frames[slot].capacity_reservation=reservation;
  }
  state.prewarm_items=items;state.prewarm_maps=PrewarmMapCapacity;state.bytes_dirty=true;
  state.stats.tlas_allocations+=SceneSnapshotCount;state.stats.snapshot_record_allocations+=SceneSnapshotCount;
  state.refresh_bytes(r);
  std::printf("world_ray_prewarm items=%u maps=%u snapshots=%u frames=%u instances=%u bytes=%llu cpu_ms=%.3f maximum_call_ms=%.3f os_budget_available=%u usage_before=%llu usage_after=%llu\n",
      items,PrewarmMapCapacity,SceneSnapshotCount,SceneFrameCount,plan->instances,
      static_cast<unsigned long long>(plan->total_bytes),
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),maximum_call_ms,
      after.budget_available?1u:0u,static_cast<unsigned long long>(before.local_usage),static_cast<unsigned long long>(after.local_usage));
  return true;
}
}
