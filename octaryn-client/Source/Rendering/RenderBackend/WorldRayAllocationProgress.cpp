#include "WorldRayTracingState.h"
namespace octaryn::client::rendering {
namespace {
struct SubmissionCpuCharge {
  FrameBuildBudget& budget;
  const std::uint64_t start{SDL_GetTicksNS()};
  ~SubmissionCpuCharge() {budget.cpu_ns+=SDL_GetTicksNS()-start;}
};
}
bool WorldRayTracing::State::progress_allocations(WorldRenderer& r,std::uint64_t budget_ns) {
  if(!budget_ns)return true;
  SubmissionCpuCharge charge{submission_budget};
  if(allocator) {
    if(allocator->pending())bytes_dirty=true;
    allocator->collect();
  }
  for(auto& job:jobs) {
    if(!job.pending || !job.allocation)continue;
    // No geometry feeder exists yet; every surviving job is cancelled work.
    if(!job.cancelled) {bytes_dirty=true;job.cancelled=true;}
    auto& allocation=*job.allocation;
    if(job.cancelled)allocation.cancelled.store(true,std::memory_order_relaxed);
    if(!allocation.complete.load(std::memory_order_acquire))continue;
    if(job.cancelled) {
      ++stats.discarded_builds;job.allocation.reset();job.pending.reset();bytes_dirty=true;continue;
    }
    RayPrepareDiagnostics diagnostic{"blas_allocation_progress"};
    diagnostic.frame=r.frames;diagnostic.generation=generation;
    diagnostic.x=job.coordinate.first;diagnostic.z=job.coordinate.second;
    diagnostic.faces=job.pending->record.face_count;
    if(!diagnostic.check(allocation.step,allocation.result))return false;
    const auto elapsed=SDL_GetTicksNS()-charge.start;
    if(elapsed>=budget_ns)break;
    if(!submission_budget.allows(job.pending->record.face_count,elapsed))continue;
    job.bounds=std::move(allocation.bounds);job.scratch=std::move(allocation.scratch);
    job.pending->blas=std::move(allocation.blas);
    allocation.blas_bytes.store(0,std::memory_order_relaxed);allocation.buffer_bytes.store(0,std::memory_order_relaxed);
    job.allocation.reset();bytes_dirty=true;
    if(!submit(r,job))return false;
    submission_budget.consumed(job.pending->record.face_count);
  }
  return true;
}
bool world_ray_progress(WorldRenderer& r,double budget_ms) {
  if(!(budget_ms>0) || r.retirement_started || !r.ray_enabled || !world_ray_available(r))return true;
  // No fence wait, scene publication, snapshot rebuild or new ticket admission.
  const auto budget_ns=static_cast<std::uint64_t>(std::min(budget_ms,2.0)*1000000.0);
  auto& state=*r.ray_tracing->state;const auto before=state.stats.blas_builds;
  const bool success=state.progress_allocations(r,budget_ns);
  state.stats.blas_private_submissions+=state.stats.blas_builds-before;
  return success;
}
}
