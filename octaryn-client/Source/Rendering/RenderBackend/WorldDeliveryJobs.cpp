#include "WorldRendererInternal.h"
#include "WorldDeliveryJobs.h"
#include "WorldMeshInput.h"
#include <cstdlib>
#include <optional>
#include <span>
#include <chrono>
namespace octaryn::client::rendering {
namespace {
using Source=world_presentation::StreamColumn;
struct WorkBudget {
  std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
  double milliseconds;
  bool available() const {
    return milliseconds>0 && std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<milliseconds;
  }
};
enum class HaloChange { None, Arrival, Urgent };
struct Delivery {
  WorldMeshJob mesh;
  std::optional<Source> source;
  std::array<std::optional<Source>,8> neighbors;
  WorldColumnGpu result;
  bool complete{},cancelled{};
  HaloChange neighborhood_change(const WorldRenderer& r) const {
    auto change=HaloChange::None;
    std::size_t index{};
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
      if(!dx && !dz)continue;
      const auto found=r.sources.find({source->x+dx,source->z+dz});
      const auto neighbor_index=index++;
      const auto& previous=neighbors[neighbor_index];
      if(result.preloaded_neighbors[neighbor_index] &&
          (std::abs(std::int64_t(source->x)+dx-r.center_x)>r.radius ||
           std::abs(std::int64_t(source->z)+dz-r.center_z)>r.radius))return HaloChange::Urgent;
      if(!previous) {
        if(found!=r.sources.end() && !world_mesh_preload_matches(result.preloaded_neighbors[neighbor_index],found->second))
          change=HaloChange::Arrival;
        continue;
      }
      if(found==r.sources.end())return HaloChange::Urgent;
      const auto& next=found->second;
      if(previous->epoch!=next.epoch || previous->revision!=next.revision || previous->min_y!=next.min_y ||
          previous->height!=next.height || previous->blocks.storage_identity()!=next.blocks.storage_identity())return HaloChange::Urgent;
    }
    return change;
  }
};
bool wanted(const WorldRenderer& r,const Source& source) {
  return std::abs(std::int64_t(source.x)-r.center_x)<=r.radius &&
      std::abs(std::int64_t(source.z)-r.center_z)<=r.radius;
}
}
struct WorldDeliveryJobs::State {
  std::array<std::unique_ptr<Delivery>,Capacity> slots;
  std::array<std::size_t,Capacity> order{};
  std::size_t count{},next_progress{};
  bool advance(WorldRenderer& r,WorkBudget& budget) {
    for(std::size_t visited=0;visited<slots.size() && budget.available();++visited) {
      auto& job=slots[next_progress];next_progress=(next_progress+1)%slots.size();
      if(job && job->source && !job->complete && !job->mesh.poll(r,job->result,job->complete))return false;
    }
    return true;
  }
  bool fill(WorldRenderer& r,world_presentation::WorldStream& stream,WorkBudget& budget,bool publish_metadata);
};
WorldDeliveryJobs::WorldDeliveryJobs():state_(std::make_unique<State>()) {}
WorldDeliveryJobs::~WorldDeliveryJobs()=default;
std::size_t WorldDeliveryJobs::pending() const {return state_->count;}
std::size_t WorldDeliveryJobs::completed() const {
  std::size_t count{};for(const auto& job:state_->slots)count+=job && job->source && job->complete?1u:0u;return count;
}
std::size_t WorldDeliveryJobs::cancelled() const {
  std::size_t count{};for(const auto& job:state_->slots)count+=job && job->source && job->cancelled?1u:0u;return count;
}
WorldMeshJobResources WorldDeliveryJobs::resources(std::size_t slot) const {
  return slot<Capacity && state_->slots[slot]?state_->slots[slot]->mesh.resources():WorldMeshJobResources{};
}
bool WorldDeliveryJobs::wait(std::size_t slot,std::uint64_t timeout) {
  return slot>=Capacity || !state_->slots[slot] || !state_->slots[slot]->source || state_->slots[slot]->mesh.wait(timeout);
}
std::uint64_t WorldDeliveryJobs::gpu_bytes() const {
  std::uint64_t bytes{};
  for(const auto& job:state_->slots)if(job) {
    bytes+=job->mesh.gpu_bytes();
    for(auto* buffer:{job->result.faces.get(),job->result.fluids.get(),job->result.patches.get()})
      if(buffer)bytes+=buffer->getDesc().size;
  }
  return bytes;
}
void WorldDeliveryJobs::retain_window(const WorldRenderer& r) {
  for(auto& job:state_->slots)if(job && job->source && !wanted(r,*job->source))job->cancelled=true;
}
bool WorldDeliveryJobs::progress(WorldRenderer& r,double budget_ms) {
  WorkBudget budget{std::chrono::steady_clock::now(),budget_ms};
  return state_->advance(r,budget);
}
bool WorldDeliveryJobs::prefetch(WorldRenderer& r,world_presentation::WorldStream& stream,double budget_ms) {
  WorkBudget budget{std::chrono::steady_clock::now(),budget_ms};
  return state_->advance(r,budget) && state_->fill(r,stream,budget,false);
}
bool WorldDeliveryJobs::pump(WorldRenderer& r,world_presentation::WorldStream& stream,double budget_ms) {
  WorkBudget budget{std::chrono::steady_clock::now(),budget_ms};
  if(!state_->advance(r,budget))return false;
  auto& s=*state_;
  // Only the oldest staged payload can publish, even if a later emit finished.
  while(s.count) {
    auto& job=*s.slots[s.order[0]];
    if(!job.complete)break;
    const auto publication=job.cancelled?world_presentation::StreamPublication::Retired:stream.publish(*job.source);
    if(publication==world_presentation::StreamPublication::Busy)break;
    if(publication==world_presentation::StreamPublication::Published) {
      WorldMeshTimer timer(r.gpu_profile?&r.mesh_timings.publication:nullptr);
      const auto change=job.neighborhood_change(r);
      world_mesh_invalidate_neighbors(r,*job.source);
      const auto coordinate=std::make_pair(job.source->x,job.source->z);
      auto retained=*job.source;retained.mesh_halo.reset();
      r.sources.insert_or_assign(coordinate,std::move(retained));
      world_renderer_store_column(r,coordinate,std::move(job.result));
      // Rebase pending commands; only acknowledged edits covered by this base retire.
      world_renderer_reapply_predicted_edits(r,coordinate);
      // Coalesce new residency until its neighborhood arrives; edits and
      // removals must correct previously visible boundaries without delay.
      if(change!=HaloChange::None)r.dirty.insert(coordinate);
      if(change==HaloChange::Urgent)r.dirty_urgent.insert(coordinate);
      r.status="terrain_resident";
    }
    job.source.reset();job.neighbors={};job.result={};job.complete=false;job.cancelled=false;
    std::move(s.order.begin()+1,s.order.begin()+s.count,s.order.begin());--s.count;
  }
  return s.fill(r,stream,budget,true);
}
bool WorldDeliveryJobs::State::fill(WorldRenderer& r,world_presentation::WorldStream& stream,
    WorkBudget& budget,bool publish_metadata) {
  auto& s=*this;
  // Exclude every staged payload while preserving the oldest-first mailbox.
  // Private spare-time work never publishes query or visible renderer state.
  while(s.count<s.slots.size() && budget.available()) {
    if(s.count && s.slots[s.order[0]]->cancelled)break;
    std::array<const Source*,Capacity> excluded{};
    for(std::size_t i=0;i<s.count;++i)excluded[i]=&*s.slots[s.order[i]]->source;
    Source source;
    if(!stream.peek(source,std::span<const Source* const>(excluded.data(),s.count)) || !wanted(r,source))break;
    if(world_renderer_same_authoritative_content(r,source)) {
      // Metadata is delivered through the same bounded mailbox, without
      // remeshing. Preserve ordering when an earlier mesh is in flight.
      if(s.count || !publish_metadata)break;
      const auto publication=stream.publish(source);
      if(publication==world_presentation::StreamPublication::Published)
        world_renderer_publish_column_metadata(r,source);
      else
        return true;
      continue;
    }
    std::size_t slot{};while(slot<s.slots.size() && s.slots[slot] && s.slots[slot]->source)++slot;
    if(slot==s.slots.size())break;
    if(!s.slots[slot])s.slots[slot]=std::make_unique<Delivery>();
    auto& job=*s.slots[slot];
    std::size_t index{};
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
      if(!dx && !dz)continue;
      const auto found=r.sources.find({source.x+dx,source.z+dz});
      if(found!=r.sources.end())job.neighbors[index]=found->second;
      ++index;
    }
    if(!job.mesh.start(r,source))return false;
    job.source=std::move(source);s.order[s.count++]=slot;
  }
  return true;
}
}
