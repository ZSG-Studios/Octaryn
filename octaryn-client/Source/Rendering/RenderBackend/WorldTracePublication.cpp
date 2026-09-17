#include "WorldTracePublication.h"
#include "WorldRendererInternal.h"
#include "PredictedColumn.h"
#include "octaryn_native_schedule_runtime.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace octaryn::client::rendering {
namespace {
using Coordinate=std::pair<std::int32_t,std::int32_t>;
using Source=world_presentation::StreamColumn;
using namespace voxel_tracing;
// 225 columns * at most 33 intersected chunks fits the bounded 8192-chunk owner.
bool wanted(const WorldRenderer& r,Coordinate coordinate) {
  const auto radius=std::min(r.radius,7);
  return std::abs(std::int64_t(coordinate.first)-r.center_x)<=radius &&
      std::abs(std::int64_t(coordinate.second)-r.center_z)<=radius;
}
bool same(const Source& a,const Source& b) {
  return a.epoch==b.epoch && a.authoritative_revision==b.authoritative_revision &&
      a.revision==b.revision && a.min_y==b.min_y && a.height==b.height &&
      a.blocks.storage_identity()==b.blocks.storage_identity();
}
struct Entry {Source source;std::uint64_t version{};};
struct Work {
  Coordinate coordinate;
  std::uint64_t version{};
  TraceColumnInput input;
  PreparedTraceColumn result;
  void* task{};
  static int execute(void* context) {
    auto& work=*static_cast<Work*>(context);
    try {work.result=prepare_trace_column(std::move(work.input));return 1;}
    catch(...) {return 0;}
  }
  ~Work() {if(task)octaryn_native_schedule_runtime_task_destroy(task);}
};
}
struct WorldTracePublication::State {
  void* scheduler{};
  std::map<Coordinate,Entry> bases;
  std::set<Coordinate> dirty,urgent;
  std::array<std::unique_ptr<Work>,2> jobs;
  std::uint64_t next_version{};
  TracePublicationStats counters;
  ~State() {
    for(auto& job:jobs)job.reset();
    if(scheduler)octaryn_native_schedule_runtime_destroy(scheduler);
  }
  void dirty_entry(Coordinate coordinate,bool priority) {
    bases.at(coordinate).version=++next_version;
    dirty.insert(coordinate);
    if(priority)urgent.insert(coordinate);
  }
};
WorldTracePublication::WorldTracePublication():state_(std::make_unique<State>()) {}
WorldTracePublication::~WorldTracePublication()=default;
void WorldTracePublication::offer(WorldRenderer& r,const Source& source) {
  const Coordinate coordinate{source.x,source.z};
  if(!wanted(r,coordinate))return;
  auto& s=*state_;
  const auto previous=s.bases.find(coordinate);
  if(previous!=s.bases.end() && same(previous->second.source,source))return;
  const bool priority=previous!=s.bases.end();
  s.bases.insert_or_assign(coordinate,Entry{source,0});
  s.dirty_entry(coordinate,priority);
}
void WorldTracePublication::retire(WorldRenderer& r,const Source& source) {
  auto& s=*state_;const Coordinate coordinate{source.x,source.z};
  const auto found=s.bases.find(coordinate);
  if(found==s.bases.end() || !same(found->second.source,source))return;
  s.bases.erase(found);s.dirty.erase(coordinate);s.urgent.erase(coordinate);
  r.trace_world.remove(coordinate.first,coordinate.second);
}
void WorldTracePublication::predictions_changed(WorldRenderer& r,std::int32_t x,std::int32_t z) {
  const Coordinate coordinate{x,z};auto& s=*state_;
  if(!wanted(r,coordinate))return;
  if(!s.bases.contains(coordinate)) {
    const auto base=r.prediction_bases.find(coordinate);
    if(base!=r.prediction_bases.end())offer(r,base->second);
    else if(const auto source=r.sources.find(coordinate);source!=r.sources.end())offer(r,source->second);
  }
  if(s.bases.contains(coordinate))s.dirty_entry(coordinate,true);
}
void WorldTracePublication::retain_window(WorldRenderer& r) {
  auto& s=*state_;
  for(auto it=s.bases.begin();it!=s.bases.end();) {
    if(wanted(r,it->first)) {++it;continue;}
    r.trace_world.remove(it->first.first,it->first.second);
    s.dirty.erase(it->first);s.urgent.erase(it->first);it=s.bases.erase(it);
  }
  // Retained voxel sources can enter the exact tracing window without remeshing.
  for(const auto& [coordinate,source]:r.sources)if(wanted(r,coordinate) && !s.bases.contains(coordinate)) {
    const auto base=r.prediction_bases.find(coordinate);
    offer(r,base==r.prediction_bases.end()?source:base->second);
  }
}
bool WorldTracePublication::pump(WorldRenderer& r) {
  auto& s=*state_;
  for(auto& job:s.jobs)if(job && octaryn_native_schedule_runtime_task_ready(job->task)) {
    if(octaryn_native_schedule_runtime_task_result(job->task,nullptr)<=0) {
      ++s.counters.discarded;job.reset();continue;
    }
    const auto base=s.bases.find(job->coordinate);
    if(base!=s.bases.end() && base->second.version==job->version && wanted(r,job->coordinate)) {
      const auto result=r.trace_world.publish_prepared(std::move(job->result));
      if(result.status==PublishStatus::Invalid)return false;
      if(result.status==PublishStatus::Capacity) {
        r.trace_world.remove(job->coordinate.first,job->coordinate.second);
        ++s.counters.capacity_rejections;s.dirty.insert(job->coordinate);
      } else if(result.status==PublishStatus::Stale) {
        ++s.counters.discarded;s.dirty.insert(job->coordinate);
      } else ++s.counters.published;
    } else ++s.counters.discarded;
    job.reset();
  }
  if(s.dirty.empty())return true;
  auto slot=std::find_if(s.jobs.begin(),s.jobs.end(),[](const auto& job){return !job;});
  if(slot==s.jobs.end())return true;
  auto selected=s.dirty.end();std::int64_t nearest=INT64_MAX;
  for(auto it=s.dirty.begin();it!=s.dirty.end();++it) {
    if(std::any_of(s.jobs.begin(),s.jobs.end(),[&](const auto& job){return job && job->coordinate==*it;}))continue;
    const auto dx=std::int64_t(it->first)-r.center_x,dz=std::int64_t(it->second)-r.center_z;
    const auto distance=dx*dx+dz*dz-(s.urgent.contains(*it)?1000000:0);
    if(distance<nearest) {nearest=distance;selected=it;}
  }
  if(selected==s.dirty.end())return true;
  if(!s.scheduler)s.scheduler=octaryn_native_schedule_runtime_create(SDL_GetNumLogicalCPUCores(),2);
  if(!s.scheduler)return false;
  const auto coordinate=*selected;const auto& base=s.bases.at(coordinate);
  auto predictions=r.predicted_edits;
  predictions.cover(coordinate.first,coordinate.second,base.source.authoritative_revision);
  auto composed=world_presentation::compose_predicted_column(base.source,predictions);
  auto job=std::make_unique<Work>();job->coordinate=coordinate;job->version=base.version;
  job->input=r.trace_world.prepare_input(composed);
  const octaryn_native_schedule_runtime_job description{
      "voxel_trace_occupancy",nullptr,0,nullptr,0,OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,Work::execute,job.get()};
  job->task=octaryn_native_schedule_runtime_submit_worker(s.scheduler,&description,1);
  if(!job->task)return false;
  *slot=std::move(job);s.dirty.erase(selected);s.urgent.erase(coordinate);
  return true;
}
TracePublicationStats WorldTracePublication::stats() const {
  auto result=state_->counters;result.queued=state_->dirty.size();
  for(const auto& job:state_->jobs)result.pending+=job?1u:0u;
  return result;
}
}
