#include "WorldRendererInternal.h"
#include "WorldHaloJobs.h"
#include "WorldMeshJob.h"
#include <optional>
#include <chrono>
namespace octaryn::client::rendering {
namespace {
using Coordinate=std::pair<std::int32_t,std::int32_t>;
using Source=world_presentation::StreamColumn;
struct Pending {
  Coordinate coordinate;
  std::array<std::optional<Source>,9> sources;
  Slang::ComPtr<rhi::IBuffer> previous_faces;
  WorldMeshJob mesh;
  WorldColumnGpu result;
  bool active{},complete{};
  bool matches(const WorldRenderer& r) const {
    const auto current=r.columns.find(coordinate);
    if(current==r.columns.end() || current->second.faces.get()!=previous_faces.get())return false;
    std::size_t index=0;
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx,++index) {
      const auto found=r.sources.find({coordinate.first+dx,coordinate.second+dz});
      const auto& before=sources[index];
      if(bool(before)!=(found!=r.sources.end()))return false;
      if(!before)continue;
      const auto& after=found->second;
      if(before->revision!=after.revision || before->min_y!=after.min_y || before->height!=after.height ||
          before->blocks.storage_identity()!=after.blocks.storage_identity())return false;
    }
    return true;
  }
};
}
struct WorldHaloJobs::State {
  std::array<std::unique_ptr<Pending>,Capacity> jobs;
  std::size_t next_progress{};
};
WorldHaloJobs::WorldHaloJobs():state_(std::make_unique<State>()) {}
WorldHaloJobs::~WorldHaloJobs()=default;
bool WorldHaloJobs::contains(Coordinate coordinate) const {
  for(const auto& job:state_->jobs)if(job && job->active && job->coordinate==coordinate)return true;
  return false;
}
std::size_t WorldHaloJobs::pending() const {
  std::size_t count=0;for(const auto& job:state_->jobs)count+=job && job->active?1u:0u;return count;
}
std::uint64_t WorldHaloJobs::gpu_bytes() const {
  std::uint64_t bytes=0;
  for(const auto& job:state_->jobs)if(job) {
    bytes+=job->mesh.gpu_bytes();
    for(auto* buffer:{job->result.faces.get(),job->result.fluids.get(),job->result.patches.get()})
      if(buffer)bytes+=buffer->getDesc().size;
  }
  return bytes;
}
WorldMeshJobResources WorldHaloJobs::resources(std::size_t slot) const {
  return slot<state_->jobs.size() && state_->jobs[slot]?state_->jobs[slot]->mesh.resources():WorldMeshJobResources{};
}
bool WorldHaloJobs::current(std::size_t slot,const WorldRenderer& r) const {
  return slot<state_->jobs.size() && state_->jobs[slot] && state_->jobs[slot]->active && state_->jobs[slot]->matches(r);
}
bool WorldHaloJobs::wait(std::size_t slot,std::uint64_t timeout) {
  return slot>=state_->jobs.size() || !state_->jobs[slot] || !state_->jobs[slot]->active ||
      state_->jobs[slot]->mesh.wait(timeout);
}
bool WorldHaloJobs::progress(WorldRenderer& r,double budget_ms) {
  if(budget_ms<=0)return true;
  const auto start=std::chrono::steady_clock::now();
  for(std::size_t visited=0;visited<Capacity;++visited) {
    auto& job=state_->jobs[state_->next_progress];
    state_->next_progress=(state_->next_progress+1)%Capacity;
    if(job && job->active && !job->complete)
      if(!job->mesh.poll(r,job->result,job->complete))return false;
    if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>=budget_ms)break;
  }
  return true;
}
bool WorldHaloJobs::pump(WorldRenderer& r,double start_budget_ms,double progress_budget_ms) {
  if(start_budget_ms<=0)return true;
  if(!progress(r,progress_budget_ms))return false;
  for(auto& job:state_->jobs)if(job && job->active && job->complete) {
    if(job->matches(r)) {
      WorldMeshTimer timer(r.gpu_profile?&r.mesh_timings.publication:nullptr);
      world_renderer_store_column(r,job->coordinate,std::move(job->result));
      if(r.gpu_profile)++r.mesh_timings.halo_published;
    }
    else if(r.sources.contains(job->coordinate) && r.columns.contains(job->coordinate)) {
      // Already-started corrections must finish even when the current window
      // still awaits new neighbors. A stale retry cannot return to loading delay.
      r.dirty.insert(job->coordinate);
      r.dirty_urgent.insert(job->coordinate);
      if(r.gpu_profile)++r.mesh_timings.halo_discarded;
    }
    else if(r.gpu_profile)++r.mesh_timings.halo_discarded;
    {
      WorldMeshTimer timer(r.gpu_profile?&r.mesh_timings.release:nullptr);
      job->previous_faces.setNull();job->sources={};job->result={};
    }
    job->active=false;job->complete=false; // Scratch and monotonic fence remain reusable.
  }
  // Bound CPU work instead of tying throughput to one column per display frame.
  // The fixed slots bound scratch and completed results even under GPU pressure.
  const auto start=std::chrono::steady_clock::now();
  std::size_t started{};
  for(auto& slot:state_->jobs)if(!slot || !slot->active) {
    if(started && std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>=start_budget_ms)break;
    Coordinate coordinate;
    if(!world_mesh_take_pending(r,coordinate))break;
    const auto primary=r.columns.find(coordinate);
    if(primary==r.columns.end())continue;
    if(!slot)slot=std::make_unique<Pending>();
    auto& job=slot;job->coordinate=coordinate;job->previous_faces=primary->second.faces;
    std::size_t index=0;
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx,++index) {
      const auto found=r.sources.find({coordinate.first+dx,coordinate.second+dz});
      if(found!=r.sources.end())job->sources[index]=found->second;
    }
    if(!job->sources[4] || !job->mesh.start(r,*job->sources[4]))return false;
    job->active=true;++started;
  }
  return true;
}
bool world_mesh_has_pending(const WorldRenderer& r) {
  return (r.delivery_jobs && r.delivery_jobs->pending()!=0) || !r.dirty.empty() || (r.halo_jobs && r.halo_jobs->pending()!=0);
}
}
