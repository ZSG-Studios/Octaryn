#include "SceneCollisionState.h"
#include "SceneBounds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace octaryn::character_motion {
namespace {constexpr auto keep_time=std::chrono::seconds(2);}
SceneCollisionResidency::SceneCollisionResidency():state_(std::make_unique<State>()) {}
SceneCollisionResidency::~SceneCollisionResidency()=default;
bool SceneCollisionResidency::load(const std::filesystem::path& catalog,const std::filesystem::path& source,
    const std::filesystem::path& scratch,std::uint64_t budget_bytes,const std::atomic_bool* cancel) {
  auto& state=*state_;
  if(state.scheduler || budget_bytes<64ull*1024*1024 || budget_bytes>4096ull*1024*1024) {
    state.error="scene collision configuration invalid";return false;
  }
  if(!read_scene_collision_catalog(catalog,source,state.catalog,state.error,cancel) ||
      !state.index.reset(state.catalog.parts,state.catalog.instances,state.error) ||
      !state.reader.open(source,scratch,state.error,&state.canceled))return false;
  state.statistics.budget_bytes=budget_bytes;
  state.scheduler=octaryn_native_schedule_runtime_create(1,1);
  if(!state.scheduler) {state.error="scene collision scheduler creation failed";return false;}
  std::printf("scene_collision_loaded parts=%zu instances=%zu budget_bytes=%llu source_expansion=0\n",
      state.catalog.parts.size(),state.catalog.instances.size(),static_cast<unsigned long long>(budget_bytes));
  return true;
}
bool SceneCollisionResidency::ready(float x,float y,float z,float radius) {
  if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(radius) || radius<0 || radius>4096)return false;
  scene_geometry::Query query;query.camera=query.actor={x,y,z};query.load_radius=std::max(radius,.01f);
  query.keep_radius=query.load_radius+8;query.actor_radius=query.load_radius;query.budget_bytes=UINT64_MAX;
  return state_->ready(query);
}
bool SceneCollisionResidency::ready_bounds(const std::array<float,6>& bounds) {
  if(!scene_geometry::bounds::valid(bounds))return false;
  scene_geometry::Query query;query.region=query.keep_region=bounds;query.budget_bytes=UINT64_MAX;
  for(unsigned axis=0;axis<3;++axis) {(*query.keep_region)[axis]-=8;(*query.keep_region)[axis+3]+=8;}
  return state_->ready(query);
}
bool SceneCollisionResidency::State::ready(const scene_geometry::Query& query) {
  auto& state=*this;
  if(!state.scheduler || !state.error.empty())return false;
  if(!state.poll())return false;
  const auto now=State::Clock::now();state.current.clear();
  for(const auto& [key,entry]:state.entries)if(entry.resident) {
    const auto part=std::uint32_t(key>>32),instance=std::uint32_t(key);
    if(state.current.empty() || state.current.back().part!=part)state.current.push_back({part,{}});
    state.current.back().instances.push_back(instance);
  }
  const auto plan=state.index.plan(query,state.current);
  if(!plan.admitted) {
    state.error=plan.error.empty()?"scene collision spatial bounds require preparation":plan.error;
    ++state.statistics.failed;return false;
  }
  state.required.clear();
  for(const auto& selection:plan.wanted)for(const auto instance:selection.instances) {
    const auto key=(std::uint64_t(selection.part)<<32)|instance;
    auto& entry=state.entries[key];entry.required=entry.wanted=now+keep_time;state.required.push_back(key);
  }
  for(const auto& selection:plan.retained)for(const auto instance:selection.instances) {
    const auto key=(std::uint64_t(selection.part)<<32)|instance;
    if(auto found=state.entries.find(key);found!=state.entries.end())found->second.wanted=now+keep_time;
  }
  std::uint64_t protected_bytes{};unsigned protected_count{};
  for(const auto& [key,entry]:state.entries)if(entry.required>=now) {
    protected_bytes+=state.reservation(std::uint32_t(key>>32));++protected_count;
  }
  if(protected_bytes>state.statistics.budget_bytes || protected_count>256) {
    state.error="scene protected collision neighborhood exceeds residency budget";++state.statistics.failed;return false;
  }
  for(auto entry=state.entries.begin();entry!=state.entries.end();) {
    if(entry->second.required>=now || entry->second.wanted>=now || (state.job.task && state.job.key==entry->first)) {++entry;continue;}
    if(entry->second.resident) {state.collision->remove_tile(entry->first);++state.statistics.evictions;}
    entry=state.entries.erase(entry);
  }
  state.refresh();
  if(!state.job.task)for(const auto key:state.required) {
    if(state.entries[key].resident)continue;
    const auto bytes=state.reservation(std::uint32_t(key>>32));
    if(bytes>state.statistics.budget_bytes-state.statistics.resident_bytes) {
      for(auto entry=state.entries.begin();entry!=state.entries.end();) {
        if(entry->second.required>=now || !entry->second.resident) {++entry;continue;}
        state.collision->remove_tile(entry->first);++state.statistics.evictions;entry=state.entries.erase(entry);
      }
      state.refresh();
    }
    if(bytes<=state.statistics.budget_bytes-state.statistics.resident_bytes && !state.start(key))return false;
    break;
  }
  state.refresh();
  const bool complete=std::all_of(state.required.begin(),state.required.end(),[&](auto key){return state.entries.at(key).resident;});
  if(!complete)++state.statistics.waits;
  return complete;
}
std::shared_ptr<MeshCollisionScene> SceneCollisionResidency::scene() const {return state_->collision;}
SceneCollisionStats SceneCollisionResidency::stats() const {return state_->statistics;}
const std::string& SceneCollisionResidency::error() const {return state_->error;}
std::uint64_t SceneCollisionResidency::triangle_count() const {
  std::uint64_t count{};for(const auto& [key,entry]:state_->entries)if(entry.resident)count+=entry.triangles;return count;
}
}
