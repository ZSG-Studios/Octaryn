#include "TileSessionInternal.h"
#include "SceneCollisionBodies.h"
#include "WorldRenderer.h"
#include "TileRayWorkBudget.h"
#include "TileDesiredSet.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <charconv>
#include <cstdlib>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
float distance(const app::WorldTile& tile,float x,float y,float z) {
  const float point[3]={x,y,z};float squared=0;
  for(unsigned axis=0;axis<3;++axis) {
    const float delta=std::max({tile.bounds[axis]-point[axis],point[axis]-tile.bounds[axis+3],0.f});
    squared+=delta*delta;
  }
  return std::sqrt(squared);
}
std::uint64_t map_bytes(const MapRenderer& map) {
  const auto bytes=map_memory_stats(map);return bytes.geometry+bytes.acceleration+bytes.scratch+bytes.reserved;
}
}
TileSession::TileSession():state_(std::make_unique<State>()) {}
TileSession::~TileSession()=default;
bool TileSession::load(const std::filesystem::path& manifest,rhi::IDevice* device,rhi::ICommandQueue* queue,TileStreamBudget budget) {
  auto& s=*state_;
  if(!s.tiles.load(manifest)){s.error="tile_manifest_invalid";return false;}
  auto physics_path=s.tiles.source_path();physics_path.replace_extension(".physics.json");
  if(!character_motion::read_scene_body_exclusions(physics_path,s.excluded_nodes,s.error))return false;
  if(s.tiles.gpu_budget_mib())budget.gpu_bytes=std::uint64_t(s.tiles.gpu_budget_mib())*1024*1024;
  if(const char* value=std::getenv("OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB")) {
    unsigned mib{};const char* end=value+std::strlen(value);
    const auto parsed=std::from_chars(value,end,mib);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || mib<64 || mib>32768) {
      s.error="tile_gpu_budget_must_be_64_to_32768_MiB";return false;
    }
    budget.gpu_bytes=std::uint64_t(mib)*1024*1024;
  }
  if(s.scheduler || !device || !queue || !std::isfinite(budget.load_radius) ||
      !std::isfinite(budget.keep_radius) || budget.load_radius<=0 || budget.keep_radius<=budget.load_radius ||
      !std::isfinite(budget.actor_radius) || budget.actor_radius<3 || !budget.gpu_bytes ||
      budget.preparation_bytes<256ull*1024*1024 || budget.preparation_bytes>512ull*1024*1024 ||
      budget.upload_bytes<65536 || !std::isfinite(budget.upload_ms) || budget.upload_ms<=0 ||
      !std::isfinite(budget.readiness_ms) || budget.readiness_ms<=0) {
    s.error="tile_stream_configuration_invalid";return false;
  }
  if(const char* value=std::getenv("OCTARYN_CLIENT_TILE_AS_INFLIGHT")) {
    unsigned capacity{};const char* end=value+std::strlen(value);
    const auto parsed=std::from_chars(value,end,capacity);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || !tile_ray_capacity_valid(capacity)) {
      s.error="tile_as_inflight_must_be_1_or_4";return false;
    }
    s.ray_capacity=capacity;
  }
  std::printf("tile_ray_policy capacity=%u max_operations=1 soft_budget_ms=%.3f\n",s.ray_capacity,budget.upload_ms);
  if(const char* value=std::getenv("OCTARYN_CLIENT_TILE_TEXTURE_REUSE")) {
    if(std::strcmp(value,"0") && std::strcmp(value,"1")) {s.error="tile_texture_reuse_must_be_0_or_1";return false;}
    s.texture_reuse_enabled=std::strcmp(value,"0")!=0;
  }
  std::printf("tile_texture_reuse enabled=%u publication=fence_complete cache_namespace=world_immutable\n",
      s.texture_reuse_enabled?1u:0u);
  s.device=device;s.queue=queue;s.budget=budget;
  s.textures=create_map_texture_pool();s.collision=std::make_shared<character_motion::MeshCollisionScene>();
  if(!prewarm_map_pipeline_pool(*s.textures,device,rhi::Format::RGBA16Float,rhi::Format::D32Float)) {
    s.error="tile_pipeline_prewarm_failed";return false;
  }
  s.scheduler=octaryn_native_schedule_runtime_create(2,2);
  if(!s.scheduler){s.error="tile_scheduler_create_failed";return false;}
  if(device->hasFeature(rhi::Feature::AccelerationStructure)) {
    rhi::BufferDesc query{};query.size=256;query.elementSize=4;
    query.usage=rhi::BufferUsage::AccelerationStructureBuildInput;
    query.defaultState=rhi::ResourceState::AccelerationStructureBuildInput;
    if(SLANG_FAILED(device->createBuffer(query,nullptr,s.size_query_address.writeRef()))) {
      s.error="tile_size_query_address_failed";return false;
    }
  }
  s.entries.resize(s.tiles.tile_count());s.priority.resize(s.entries.size());
  if(s.tiles.external_residency() && !tile_desired_set(s.tiles.tile_count(),s.tiles.initial_wanted(),s.tiles.initial_wanted(),s.external_wanted,s.external_retained)) {
    s.error="tile_initial_desired_set_invalid";return false;
  }
  for(unsigned i=0;i<s.priority.size();++i)s.priority[i]=i;
  std::printf("tile_budget gpu_bytes=%llu upload_bytes=%llu upload_ms=%.3f load_radius=%.1f keep_radius=%.1f\n",
      static_cast<unsigned long long>(budget.gpu_bytes),static_cast<unsigned long long>(budget.upload_bytes),
      budget.upload_ms,budget.load_radius,budget.keep_radius);
  return true;
}
void TileSession::State::evict(unsigned index) {
  auto& entry=entries[index];
  collision->remove_tile(index);
  if(entry.map)retired.push_back({std::move(entry.map),last_fence,last_signal,
      entry.ray_active?entry.reserved:0});
  if(entry.ray_active)++retired_ray_pending;
  entry.phase=Phase::Absent;entry.reserved=0;entry.ray_budget_checked=false;entry.ray_active=false;
  tiles.mutable_tile(index)->resident=false;
  ++statistics.evicted;changed=true;
}
bool TileSession::State::collect_retired() {
  retired_ray_pending=0;
  std::erase_if(retired,[&](const auto& item) {
    if(!map_ray_retirement_ready(*item.map)) {++retired_ray_pending;return false;}
    if(!item.fence)return item.map.use_count()==1;
    std::uint64_t value{};
    if(SLANG_FAILED(item.fence->getCurrentValue(&value)) || value==UINT64_MAX) {error="tile_retirement_fence_failed";return false;}
    return value>=item.signal && item.map.use_count()==1;
  });
  return error.empty();
}
void TileSession::State::decide(const WorldCamera& camera,const WorldCamera& actor) {
  statistics.wanted=0;priority.clear();
  const auto now=std::chrono::steady_clock::now();
  for(unsigned i=0;i<entries.size();++i) {
    auto& entry=entries[i];const auto& tile=*tiles.tile(i);
    if(tiles.external_residency()) {
      entry.wanted=external_wanted[i];entry.keep=external_retained[i];
      if(!entry.wanted && entry.phase==Phase::Absent) {
        entry.requested_at={};entry.deadline_reported=false;continue;
      }
    }
    const float camera_distance=distance(tile,camera.x,camera.y,camera.z);
    const float actor_distance=distance(tile,actor.x,actor.y,actor.z);
    entry.wanted=tiles.external_residency()?external_wanted[i]:camera_distance<=budget.load_radius || actor_distance<=budget.actor_radius;
    if(!entry.wanted) {entry.requested_at={};entry.deadline_reported=false;}
    else if(entry.requested_at==std::chrono::steady_clock::time_point{})entry.requested_at=now;
    else if(entry.phase!=Phase::Ready && !entry.deadline_reported &&
        std::chrono::duration<double,std::milli>(now-entry.requested_at).count()>budget.readiness_ms) {
      entry.deadline_reported=true;++statistics.deadline_misses;
      std::printf("tile_readiness_deadline id=%u phase=%u deadline_ms=%.3f collision_ready=0\n",
          i,unsigned(entry.phase),budget.readiness_ms);
    }
    entry.keep=tiles.external_residency()?external_retained[i]:camera_distance<=budget.keep_radius || actor_distance<=budget.actor_radius+8;
    entry.priority=actor_distance<=budget.actor_radius?actor_distance-budget.keep_radius:camera_distance;
    statistics.wanted+=entry.wanted?1u:0u;
    if(entry.phase==Phase::Ready && !entry.keep)evict(i);
    if(entry.phase==Phase::Prepared && !entry.wanted) {
      entry.prepared.reset();entry.collision.reset();entry.phase=Phase::Absent;entry.reserved=0;++statistics.cancelled;
    }
    if(entry.phase==Phase::Uploading && !entry.wanted) {
      entry.cancelled_upload=true;cancel_map_renderer_build(entry.builder);
    }
    if(entry.wanted || entry.phase!=Phase::Absent)priority.push_back(i);
  }
  for(auto& job:jobs)if(job.task && !entries[job.tile].wanted)job.cancelled=true;
  std::stable_sort(priority.begin(),priority.end(),[&](unsigned a,unsigned b){return entries[a].priority<entries[b].priority;});
}
void TileSession::State::refresh() {
  statistics.resident=statistics.preparing=statistics.uploading=0;
  statistics.resident_bytes=statistics.reserved_bytes=statistics.retired_bytes=0;
  statistics.preparation_reserved_bytes=0;
  for(const auto& entry:entries) {
    if(entry.phase==Phase::Ready) {
      ++statistics.resident;const auto allocated=map_bytes(*entry.map);statistics.resident_bytes+=allocated;
      if(entry.ray_budget_checked && !map_ray_ready(*entry.map) && entry.reserved>allocated)
        statistics.reserved_bytes+=entry.reserved-allocated;
    }
    if(entry.phase==Phase::Loading || entry.phase==Phase::Prepared)++statistics.preparing;
    if(entry.phase==Phase::Loading || entry.phase==Phase::Prepared || entry.phase==Phase::Uploading)
      statistics.preparation_reserved_bytes+=256ull*1024*1024;
    if(entry.phase==Phase::Uploading || entry.phase==Phase::Ray) {++statistics.uploading;statistics.reserved_bytes+=entry.reserved;}
  }
  for(const auto& item:retired)statistics.retired_bytes+=std::max(map_bytes(*item.map),item.reserved);
  const auto pool=map_texture_pool_stats(*textures);
  statistics.texture_bytes=pool.allocated;
}
bool TileSession::pump(const WorldCamera& camera,const WorldCamera& actor,rhi::ICommandEncoder* commands,bool ray_required,
    std::vector<std::shared_ptr<MapRenderer>>& published,const MapRaySubmitScope* profile) {
  auto& s=*state_;if(!s.scheduler || !commands || !s.error.empty())return false;
  if(s.commands_recorded){s.error="tile_upload_submission_not_acknowledged";return false;}
  s.changed=false;++s.frame;
  if(!s.collect_retired())return false;
  s.decide(camera,actor);
  if(!s.poll_jobs())return false;
  s.refresh();
  if(!s.progress(commands,ray_required,profile) || !s.start_jobs())return false;
  if(s.changed) {
    published.clear();
    for(const auto& entry:s.entries)if(entry.phase==State::Phase::Ready)published.push_back(entry.map);
    ++s.statistics.generation;
  }
  s.refresh();
  if(s.changed || s.frame%120==0) {
    const auto& stats=s.statistics;
    std::printf("tile_stream frame=%llu wanted=%u priority_entries=%zu catalogue_entries=%zu resident=%u preparing=%u uploading=%u resident_bytes=%llu reserved_bytes=%llu retired_bytes=%llu texture_bytes=%llu cancelled=%u evicted=%u generation=%llu camera_x=%.3f texture_reuses=%llu avoided_dds_bytes=%llu\n",
        static_cast<unsigned long long>(s.frame),stats.wanted,s.priority.size(),s.entries.size(),stats.resident,stats.preparing,stats.uploading,
        static_cast<unsigned long long>(stats.resident_bytes),static_cast<unsigned long long>(stats.reserved_bytes),
        static_cast<unsigned long long>(stats.retired_bytes),static_cast<unsigned long long>(stats.texture_bytes),
        stats.cancelled,stats.evicted,static_cast<unsigned long long>(stats.generation),camera.x,
        static_cast<unsigned long long>(s.texture_reuses),static_cast<unsigned long long>(s.avoided_dds_bytes));
    std::fflush(stdout);
  }
  return true;
}
void TileSession::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;s.last_fence=fence;s.last_signal=value;
  if(s.recorded) {map_renderer_build_submitted(s.recorded,fence,value);s.recorded=nullptr;}
  s.commands_recorded=false;
}
bool TileSession::commands_recorded() const {return state_->commands_recorded;}
bool TileSession::capture_ready() const {
  const auto& s=*state_;
  if(!s.error.empty() || !s.statistics.resident || !s.statistics.wanted ||
      s.statistics.preparing || s.statistics.uploading)return false;
  for(const auto& entry:s.entries)if(entry.wanted && entry.phase!=State::Phase::Ready)return false;
  return true;
}
bool TileSession::collision_ready(float x,float y,float z,float radius) const {
  const auto& s=*state_;bool inside=false;
  for(unsigned i=0;i<s.tiles.tile_count();++i) {
    const auto& tile=*s.tiles.tile(i);if(!tile.collision)continue;
    // Unselected external alternatives are not required collision.
    if(s.tiles.external_residency() && !s.entries[i].wanted)continue;
    const float delta=distance(tile,x,y,z);
    if(delta<=radius && !s.collision->contains(i))return false;
    inside|=x>=tile.bounds[0] && x<=tile.bounds[3] && z>=tile.bounds[2] && z<=tile.bounds[5];
  }
  return inside;
}
std::shared_ptr<character_motion::MeshCollisionScene> TileSession::collision_scene() const {return state_->collision;}
TileStreamStats TileSession::stats() const {return state_->statistics;}
const char* TileSession::error() const {return state_->error.c_str();}
}
