#include "WorldRayTracingState.h"
#include <cstddef>
#include <cstring>
namespace octaryn::client::rendering {
void WorldRayTracing::State::refresh_bytes(const WorldRenderer& r) {
    if(!bytes_dirty)return;
    stats.blas_bytes=stats.tlas_bytes=stats.temporary_bytes=stats.retired_mesh_bytes=0;
    std::set<const Column*> owners;std::set<const Snapshot*> scenes;
    for(const auto& [coord,column]:columns)owners.insert(column.get());
    for(const auto& [coord,column]:changed)owners.insert(column.get());
    for(const auto& job:jobs) {
      if(job.pending)owners.insert(job.pending.get());
      if(job.refit_source)owners.insert(job.refit_source.get());
      for(auto* resource:{job.bounds.get(),job.scratch.get()})if(resource)stats.temporary_bytes+=resource->getDesc().size;
    }
    if(current)scenes.insert(current.get());
    for(const auto& frame:frames) {
      if(frame.snapshot)scenes.insert(frame.snapshot.get());
      if(frame.update_source)scenes.insert(frame.update_source.get());
      for(auto* resource:{frame.instances.get(),frame.scratch.get(),frame.dummy_bounds.get(),frame.dummy_scratch.get()})
        if(resource)stats.temporary_bytes+=resource->getDesc().size;
    }
    for(auto* scene:scenes) {
      stats.tlas_bytes+=scene->tlas->getDesc().size+scene->records->getDesc().size;
      for(const auto& column:scene->columns)owners.insert(column.get());
    }
    for(auto* column:owners)stats.blas_bytes+=column->blas->getDesc().size;
    std::set<rhi::IBuffer*> retired_meshes;
    for(auto* column:owners) {
      const Coord coordinate{static_cast<std::int32_t>(column->record.reserved[0]),
        static_cast<std::int32_t>(column->record.reserved[1])};
      const auto resident=r.columns.find(coordinate);
      if(column->faces && (resident==r.columns.end() || resident->second.faces.get()!=column->faces.get()))
        retired_meshes.insert(column->faces.get());
      if(column->fluids && (resident==r.columns.end() || resident->second.fluids.get()!=column->fluids.get()))
        retired_meshes.insert(column->fluids.get());
    }
    for(auto* mesh:retired_meshes)stats.retired_mesh_bytes+=mesh->getDesc().size;
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
  if(mode && !std::strcmp(mode,"off")) {std::puts("world_ray mode=off");return true;}
  if(!r.capabilities.inline_lighting()) {
    std::puts("world_ray mode=unavailable reason=device_features");
    return !mode || std::strcmp(mode,"required");
  }
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/RayTracing/WorldRayBounds.slang","main",s.bounds_pipeline))return false;
  rhi::FenceDesc fence{};fence.label="world_ray_build_completion";
  if(!world_rhi_ok(r.device->createFence(fence,s.fence.writeRef())))return false;
  s.available=true;std::puts("world_ray mode=inline_query geometry=exact_quads coverage=all_resident max_jobs=4 builds_per_frame=2 face_budget=262144");
  return true;
}
bool world_ray_available(const WorldRenderer& r) {return r.ray_tracing && r.ray_tracing->state->available;}
bool world_ray_prepare(WorldRenderer& r,rhi::ICommandEncoder* commands,unsigned slot) {
  if(!world_ray_available(r))return true;
  auto& s=*r.ray_tracing->state;
  if(slot>=s.frames.size() || !commands) {std::fprintf(stderr,"ray_prepare_failed step=slot slot=%u frames=%u commands=%p\n",slot,unsigned(s.frames.size()),static_cast<void*>(commands));return false;}
  s.active_slot=slot;auto& frame=s.frames[slot];
  RayPrepareDiagnostics diagnostic{"prepare"};diagnostic.frame=r.frames;diagnostic.generation=s.generation;
  if(!frame.timing.resolve(s.stats.tlas_gpu_ms,&diagnostic))return false;
  if(frame.update_source || (frame.snapshot && frame.snapshot!=s.current))s.bytes_dirty=true;
  frame.snapshot.reset();frame.update_source.reset();
  for(auto it=s.columns.begin();it!=s.columns.end();) {
    const auto found=r.columns.find(it->first);
    if(found==r.columns.end() || !found->second.face_count) {
      s.built_pass_counts.erase(it->first);
      it=s.columns.erase(it);++s.generation;s.bytes_dirty=true;
    }
    else {
      // Publish replacements atomically after their build fence. Removing an
      // edited column here exposed the sky through all its unchanged walls.
      if(!it->second->matches(found->second) && s.changed.size()<64 &&
          std::none_of(s.jobs.begin(),s.jobs.end(),[&](const BuildJob& job){return job.pending && job.coordinate==it->first;}))
        s.changed.insert_or_assign(it->first,it->second);
      ++it;
    }
  }
  for(auto it=s.changed.begin();it!=s.changed.end();) {
    const auto found=r.columns.find(it->first);
    if(found==r.columns.end() || !found->second.face_count) {it=s.changed.erase(it);s.bytes_dirty=true;}
    else ++it;
  }
  if(!s.poll(r)) {std::fprintf(stderr,"ray_prepare_failed step=poll\n");return false;}
  if(!r.ray_enabled && s.current && s.current->generation!=s.generation) {
    s.current.reset();s.bytes_dirty=true;
  }
  s.stats.resident_columns=0;
  auto& candidates=s.candidates;candidates.clear();
  for(auto it=r.columns.begin();it!=r.columns.end();++it) {
    if(!it->second.face_count)continue;
    ++s.stats.resident_columns;
    const auto ready=s.columns.find(it->first);
    if(!r.ray_enabled || (ready!=s.columns.end() && ready->second->matches(it->second)) ||
      std::any_of(s.jobs.begin(),s.jobs.end(),[&](const BuildJob& job){return job.pending && job.coordinate==it->first;}))continue;
    const auto dx=double(it->first.first)-r.center_x,dz=double(it->first.second)-r.center_z;
    // Edited columns take precedence; new residency is ordered near the camera.
    const auto distance=(ready!=s.columns.end()?-1e12:0)+dx*dx+dz*dz;
    candidates.emplace_back(distance,it->first);
  }
  const auto free_jobs=static_cast<unsigned>(std::count_if(s.jobs.begin(),s.jobs.end(),[](const BuildJob& job){return !job.pending;}));
  const auto count=std::min<std::size_t>(candidates.size(),std::min(s.build_budget,free_jobs));
  std::partial_sort(candidates.begin(),candidates.begin()+static_cast<std::ptrdiff_t>(count),candidates.end());
  std::uint64_t faces{};
  for(std::size_t i=0;i<count;++i) {
    const auto& coordinate=candidates[i].second;const auto& source=r.columns.at(coordinate);
    if(i && faces+source.face_count>s.face_budget)break;
    if(!s.start(r,coordinate,source)) {
      std::fprintf(stderr,"ray_prepare_failed step=start resident=%zu ready=%zu candidates=%zu jobs=%u "
        "blas_bytes=%llu tlas_bytes=%llu temporary_bytes=%llu\n",r.columns.size(),s.columns.size(),candidates.size(),
        unsigned(s.jobs.size())-free_jobs,static_cast<unsigned long long>(s.stats.blas_bytes),
        static_cast<unsigned long long>(s.stats.tlas_bytes),static_cast<unsigned long long>(s.stats.temporary_bytes));
      return false;
    }
    faces+=source.face_count;
  }
  if(r.ray_enabled) {
    if(!s.snapshot(r,commands,frame)) {std::fprintf(stderr,"ray_prepare_failed step=snapshot\n");return false;}
    for(const auto& column:frame.snapshot->columns) {
      commands->setBufferState(column->faces,rhi::ResourceState::ShaderResource);
      commands->setBufferState(column->fluids,rhi::ResourceState::ShaderResource);
    }
  }
  s.stats.ready_columns=0;
  for(const auto& [coordinate,column]:s.columns) {
    const auto resident=r.columns.find(coordinate);
    if(resident!=r.columns.end() && column->matches(resident->second))++s.stats.ready_columns;
  }
  s.stats.scene_generation=s.generation;
  s.stats.pending_columns=s.stats.resident_columns-s.stats.ready_columns;
  s.stats.active_jobs=static_cast<std::uint32_t>(std::count_if(s.jobs.begin(),s.jobs.end(),[](const BuildJob& job){return bool(job.pending);}));
  s.refresh_bytes(r);
  if(r.frames%120==0) {
    const auto stats=world_ray_stats(r);
    std::printf("world_ray ready=%u pending=%u jobs=%u blas_builds=%llu tlas_builds=%llu blas_bytes=%llu tlas_bytes=%llu temporary_bytes=%llu discarded=%llu blas_refits=%llu tlas_updates=%llu scene_generation=%llu retired_mesh_bytes=%llu blas_gpu_ms=%.4f tlas_gpu_ms=%.4f\n",
      stats.ready_columns,stats.pending_columns,stats.active_jobs,static_cast<unsigned long long>(stats.blas_builds),
      static_cast<unsigned long long>(stats.tlas_builds),static_cast<unsigned long long>(stats.blas_bytes),
      static_cast<unsigned long long>(stats.tlas_bytes),static_cast<unsigned long long>(stats.temporary_bytes),
      static_cast<unsigned long long>(stats.discarded_builds),static_cast<unsigned long long>(stats.blas_refits),
      static_cast<unsigned long long>(stats.tlas_updates),static_cast<unsigned long long>(stats.scene_generation),
      static_cast<unsigned long long>(stats.retired_mesh_bytes),stats.blas_gpu_ms,stats.tlas_gpu_ms);
  }
  return true;
}
bool world_ray_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  if(!world_ray_available(r) || !r.ray_enabled || !root)return false;
  auto& s=*r.ray_tracing->state;const auto& scene=s.frames[s.active_slot].snapshot;
  if(!scene)return false;
  const std::array<float,4> settings{scene->columns.empty()?0.f:1.f,4096.f,.002f,0.f};
  // Only the water pass declares reflectionRange; tolerate its absence elsewhere.
  const auto range=rhi::ShaderCursor(root)["reflectionRange"];
  if(range.isValid() &&
     !world_rhi_ok(range.setData(&r.lighting_settings.reflection_distance,sizeof(float))))return false;
  auto rayScene=rhi::ShaderCursor(root)["rayScene"];
  auto raySettings=rhi::ShaderCursor(root)["raySettings"];
  if(rayScene.isValid() && !world_rhi_ok(rayScene.setBinding(rhi::Binding(scene->tlas))))return false;
  if(raySettings.isValid() && !world_rhi_ok(raySettings.setData(settings.data(),sizeof(settings))))return false;
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
