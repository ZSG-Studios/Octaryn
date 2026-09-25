#include "WorldRayTracingState.h"
#include "ReflectionQuality.h"
#include <cstddef>
#include <cstring>
#include <functional>
namespace octaryn::client::rendering {
namespace {
template<class T> void unique_pointers(std::vector<T*>& values) {
  std::sort(values.begin(),values.end(),std::less<T*>{});
  values.erase(std::unique(values.begin(),values.end()),values.end());
}
}
void WorldRayTracing::State::refresh_bytes(const WorldRenderer& r) {
    if(!bytes_dirty)return;
    stats.blas_bytes=stats.tlas_bytes=stats.temporary_bytes=stats.retired_mesh_bytes=0;
    auto& owners=accounting_columns;auto& scenes=accounting_snapshots;
    owners.clear();scenes.clear();
    for(const auto& [coord,column]:columns)owners.push_back(column.get());
    for(const auto& [coord,column]:changed)owners.push_back(column.get());
    for(const auto& job:jobs) {
      if(job.pending)owners.push_back(job.pending.get());
      if(job.refit_source)owners.push_back(job.refit_source.get());
      for(auto* resource:{job.bounds.get(),job.scratch.get()})if(resource)stats.temporary_bytes+=resource->getDesc().size;
    }
    if(current)scenes.push_back(current.get());
    if(spare)scenes.push_back(spare.get());
    for(const auto& frame:frames) {
      if(frame.snapshot)scenes.push_back(frame.snapshot.get());
      if(frame.update_source)scenes.push_back(frame.update_source.get());
      for(auto* resource:{frame.instances.get(),frame.scratch.get(),frame.dummy_bounds.get(),frame.dummy_scratch.get()})
        if(resource)stats.temporary_bytes+=resource->getDesc().size;
    }
    unique_pointers(scenes);
    for(auto* scene:scenes) {
      stats.tlas_bytes+=scene->tlas->getDesc().size+scene->records->getDesc().size;
      for(const auto& column:scene->columns)owners.push_back(column.get());
    }
    unique_pointers(owners);
    for(auto* column:owners)if(column->blas)stats.blas_bytes+=column->blas->getDesc().size;
    if(allocator)stats.temporary_bytes+=allocator->bytes();
    auto& retired_meshes=accounting_meshes;retired_meshes.clear();
    for(auto* column:owners) {
      if(column->faces)retired_meshes.push_back(column->faces.get());
      if(column->fluids)retired_meshes.push_back(column->fluids.get());
    }
    if(dummy)stats.blas_bytes+=dummy->getDesc().size;
    bytes_dirty=false;
  }

WorldRayTracing::WorldRayTracing():state(std::make_unique<State>()) {}
WorldRayTracing::~WorldRayTracing()=default;
bool world_ray_initialize(WorldRenderer& r) {
  r.ray_tracing=std::make_unique<WorldRayTracing>();auto& s=*r.ray_tracing->state;
  const auto* mode=SDL_getenv("OCTARYN_CLIENT_RAY_TRACING");
  if(mode && std::strcmp(mode,"auto") && std::strcmp(mode,"off") && std::strcmp(mode,"required")) {
    r.status="invalid_ray_tracing_mode";return false;
  }
  if(mode && !std::strcmp(mode,"off")) {
    std::puts("world_ray mode=off");return true;
  }
  if(!r.capabilities.inline_lighting()) {
    std::puts("world_ray mode=unavailable reason=device_features");
    return !mode || std::strcmp(mode,"required");
  }
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/RayTracing/WorldRayBounds.slang","main",s.bounds_pipeline))return false;
  rhi::FenceDesc fence{};fence.label="world_ray_build_completion";
  if(!world_rhi_ok(r.device->createFence(fence,s.fence.writeRef())))return false;
  s.allocator=std::make_unique<WorldRayAllocator>(r.device);
  s.available=true;
  std::printf("world_ray mode=inline_query geometry=exact_quads coverage=all_resident max_jobs=%u builds_per_frame=%u face_budget=%u cpu_budget_ms=%.3f\n",
      unsigned(s.jobs.size()),s.build_budget,s.face_budget,double(BuildCpuBudgetNs)/1e6);
  return true;
}
bool world_ray_available(const WorldRenderer& r) {return r.ray_tracing && r.ray_tracing->state->available;}
bool world_ray_scene_usable(const WorldRenderer& r) {
  if(!r.ray_enabled || !world_ray_available(r))return false;
  const auto& s=*r.ray_tracing->state;
  const auto& scene=s.frames[s.active_slot].snapshot;
  if(!scene || !scene->tlas || !scene->records || scene->generation!=s.generation)return false;
  if(r.map && !map_ray_ready(*r.map))return false;
  if(scene->map_blas.get()!=(r.map?map_ray_blas(*r.map):nullptr))return false;
  // The mandatory masked dummy TLAS is bindable, but contains no world scene.
  return !scene->columns.empty() || bool(scene->map_blas);
}
bool world_ray_coverage_complete(const WorldRenderer& r) {
  if(!world_ray_scene_usable(r))return false;
  const auto& s=*r.ray_tracing->state;
  const auto stats=world_ray_stats(r);
  return stats.pending_columns==0 && stats.active_jobs==0 && stats.ready_columns==stats.resident_columns &&
    s.frames[s.active_slot].snapshot->columns.size()==stats.ready_columns;
}
bool world_ray_prepare(WorldRenderer& r,rhi::ICommandEncoder* commands,unsigned slot) {
  if(!world_ray_available(r))return true;
  auto& s=*r.ray_tracing->state;
  if(slot>=s.frames.size() || !commands) {std::fprintf(stderr,"ray_prepare_failed step=slot slot=%u frames=%u commands=%p\n",slot,unsigned(s.frames.size()),static_cast<void*>(commands));return false;}
  // r.frames advances before cap work, so it cannot key this budget reset.
  s.submission_budget.reset(s.build_budget,s.face_budget);
  s.active_slot=slot;auto& frame=s.frames[slot];
  RayPrepareDiagnostics diagnostic{"prepare"};diagnostic.frame=r.frames;diagnostic.generation=s.generation;
  if(!frame.timing.resolve(s.stats.tlas_gpu_ms,&diagnostic))return false;
  if(frame.update_source || (frame.snapshot && frame.snapshot!=s.current))s.bytes_dirty=true;
  // The caller completed this slot's fence. Other frames/current may still
  // share its snapshot; only exclusive ownership permits a full rebuild here.
  std::shared_ptr<Snapshot> reusable;
  if(frame.snapshot && frame.snapshot.use_count()==1)reusable=std::move(frame.snapshot);
  frame.snapshot.reset();frame.update_source.reset();
  if(!s.poll(r)) {std::fprintf(stderr,"ray_prepare_failed step=poll\n");return false;}
  if(!r.ray_enabled && s.current && s.current->generation!=s.generation) {
    s.current.reset();s.bytes_dirty=true;
  }
  s.stats.resident_columns=0;
  if(r.ray_enabled) {
    if(!s.snapshot(r,commands,frame,std::move(reusable))) {std::fprintf(stderr,"ray_prepare_failed step=snapshot\n");return false;}
    // Published mesh buffers are immutable in their ShaderResource default.
    // Snapshot owners retain the bindless resources until this frame completes.
  } else {reusable.reset();s.spare.reset();s.bytes_dirty=true;}
  s.stats.ready_columns=0;
  s.stats.scene_generation=s.generation;
  s.stats.pending_columns=s.stats.resident_columns-s.stats.ready_columns;
  s.stats.active_jobs=static_cast<std::uint32_t>(std::count_if(s.jobs.begin(),s.jobs.end(),[](const BuildJob& job){return bool(job.pending);}));
  if(s.allocator) {
    if(s.allocator->pending())s.bytes_dirty=true;
    s.stats.blas_allocations=s.allocator->created();s.stats.blas_allocation_worker_ms=s.allocator->milliseconds();
  }
  s.refresh_bytes(r);
  if(r.frames%120==0) {
    const auto stats=world_ray_stats(r);
    std::printf("world_ray ready=%u pending=%u jobs=%u blas_builds=%llu tlas_builds=%llu blas_bytes=%llu tlas_bytes=%llu temporary_bytes=%llu discarded=%llu blas_refits=%llu tlas_updates=%llu scene_generation=%llu retired_mesh_bytes=%llu blas_gpu_ms=%.4f tlas_gpu_ms=%.4f tlas_allocations=%llu snapshot_record_allocations=%llu blas_allocations=%llu blas_allocation_worker_ms=%.6f blas_private_submissions=%llu\n",
      stats.ready_columns,stats.pending_columns,stats.active_jobs,static_cast<unsigned long long>(stats.blas_builds),
      static_cast<unsigned long long>(stats.tlas_builds),static_cast<unsigned long long>(stats.blas_bytes),
      static_cast<unsigned long long>(stats.tlas_bytes),static_cast<unsigned long long>(stats.temporary_bytes),
      static_cast<unsigned long long>(stats.discarded_builds),static_cast<unsigned long long>(stats.blas_refits),
      static_cast<unsigned long long>(stats.tlas_updates),static_cast<unsigned long long>(stats.scene_generation),
      static_cast<unsigned long long>(stats.retired_mesh_bytes),stats.blas_gpu_ms,stats.tlas_gpu_ms,
      static_cast<unsigned long long>(stats.tlas_allocations),static_cast<unsigned long long>(stats.snapshot_record_allocations),
      static_cast<unsigned long long>(stats.blas_allocations),stats.blas_allocation_worker_ms,
      static_cast<unsigned long long>(stats.blas_private_submissions));
  }
  return true;
}
bool world_ray_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  if(!world_ray_available(r) || !r.ray_enabled || !root)return false;
  auto& s=*r.ray_tracing->state;const auto& scene=s.frames[s.active_slot].snapshot;
  if(!scene)return false;
  // Presentation can query the immutable old columns while replacements build.
  // Direct shadows separately require complete current resident coverage.
  const bool geometry_ready=world_ray_scene_usable(r);
  const std::array<float,4> settings{geometry_ready?1.f:0.f,4096.f,.002f,0.f};
  // Reflection bindings are optional for other ray-query passes.
  const auto range=rhi::ShaderCursor(root)["reflectionRange"];
  if(range.isValid() &&
     !world_rhi_ok(range.setData(&r.lighting_settings.reflection_distance,sizeof(float))))return false;
  const auto sampling=rhi::ShaderCursor(root)["reflectionSampling"];
  const auto reflection_options=reflection_sampling(r.lighting_settings.reflection_quality);
  if(sampling.isValid() && !world_rhi_ok(sampling.setData(reflection_options.data(),sizeof(reflection_options))))return false;
  auto rayScene=rhi::ShaderCursor(root)["rayScene"];
  auto raySettings=rhi::ShaderCursor(root)["raySettings"];
  if(rayScene.isValid() && !world_rhi_ok(rayScene.setBinding(rhi::Binding(scene->tlas))))return false;
  if(raySettings.isValid() && !world_rhi_ok(raySettings.setData(settings.data(),sizeof(settings))))return false;
  if(r.map && !bind_map_ray_buffers(*r.map,root))return false;
  return bind_buffer(root,"rayRecords",scene->records) && bind_player_shadows(r.player,root,scene->tlas);
}
WorldRayTracingStats world_ray_stats(const WorldRenderer& r) {
  if(!r.ray_tracing)return {};
  return r.ray_tracing->state->stats;
}
void world_ray_set_build_budget(WorldRenderer& r,unsigned builds_per_frame,unsigned faces_per_frame) {
  if(!r.ray_tracing)return;
  auto& s=*r.ray_tracing->state;
  s.build_budget=std::clamp(builds_per_frame,1u,static_cast<unsigned>(s.jobs.size()));
  s.face_budget=std::max(faces_per_frame,1u);
}
}
