#include "WorldRendererInternal.h"
#include "WorldDeliveryJobs.h"
#include <cstdlib>
#include <optional>
namespace octaryn::client::rendering {
namespace {
using Source=world_presentation::StreamColumn;
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
      const auto& previous=neighbors[index++];
      if(!previous) {
        if(found!=r.sources.end())change=HaloChange::Arrival;
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
  std::array<std::unique_ptr<Delivery>,2> slots;
  std::array<std::size_t,2> order{};
  std::size_t count{};
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
  return slot<2 && state_->slots[slot]?state_->slots[slot]->mesh.resources():WorldMeshJobResources{};
}
bool WorldDeliveryJobs::wait(std::size_t slot,std::uint64_t timeout) {
  return slot>=2 || !state_->slots[slot] || !state_->slots[slot]->source || state_->slots[slot]->mesh.wait(timeout);
}
std::uint64_t WorldDeliveryJobs::gpu_bytes() const {
  std::uint64_t bytes{};
  for(const auto& job:state_->slots)if(job) {
    bytes+=job->mesh.gpu_bytes();
    for(auto* buffer:{job->result.faces.get(),job->result.arguments.get(),job->result.fluids.get(),job->result.patches.get()})
      if(buffer)bytes+=buffer->getDesc().size;
  }
  return bytes;
}
void WorldDeliveryJobs::retain_window(const WorldRenderer& r) {
  for(auto& job:state_->slots)if(job && job->source && !wanted(r,*job->source))job->cancelled=true;
}
bool WorldDeliveryJobs::progress(WorldRenderer& r) {
  for(auto& job:state_->slots)if(job && job->source && !job->complete)
    if(!job->mesh.poll(r,job->result,job->complete))return false;
  return true;
}
bool WorldDeliveryJobs::pump(WorldRenderer& r,world_presentation::WorldStream& stream) {
  if(!progress(r))return false;
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
      r.sources.insert_or_assign(coordinate,*job.source);
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
    s.order[0]=s.order[1];--s.count;
  }
  if(s.count==s.slots.size())return true;
  // An evicted head may still occupy the mailbox after a rapid return. Finish
  // cancelling its staged prefix before allowing that payload to stage again.
  if(s.count && s.slots[s.order[0]]->cancelled)return true;
  Source source;
  const auto* excluded=s.count?&*s.slots[s.order[0]]->source:nullptr;
  if(!stream.peek(source,excluded) || !wanted(r,source))return true;
  if(world_renderer_same_authoritative_content(r,source)) {
    // Metadata is delivered through the same bounded mailbox, without remeshing.
    // Finish earlier mesh payloads before allowing a later watermark to publish.
    if(s.count)return true;
    const auto publication=stream.publish(source);
    if(publication==world_presentation::StreamPublication::Published)
      world_renderer_publish_column_metadata(r,source);
    r.trace_publication.offer(r,source);
    return true;
  }
  std::size_t slot{};while(s.slots[slot] && s.slots[slot]->source)++slot;
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
  r.trace_publication.offer(r,source);
  job.source=std::move(source);s.order[s.count++]=slot;
  return true;
}
}
