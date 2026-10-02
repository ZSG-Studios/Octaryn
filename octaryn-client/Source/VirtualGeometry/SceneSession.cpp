#include "SceneSessionInternal.h"
#include "WorldGeometry.h"
#include "WorldGeometryRay.h"
#include "SceneMemoryLedger.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <set>
#include <cmath>

namespace octaryn::client::rendering {
namespace {
bool covers(const scene_geometry::Selection& resident,const scene_geometry::Selection& wanted) {
  return resident.part==wanted.part && std::includes(resident.instances.begin(),resident.instances.end(),wanted.instances.begin(),wanted.instances.end());
}
}
SceneSession::SceneSession():state_(std::make_unique<State>()) {}
SceneSession::~SceneSession()=default;
bool SceneSession::load(WorldRenderer& renderer,const std::filesystem::path& catalog,const std::filesystem::path& source,
    std::uint64_t gpu_budget,bool overlay) {
  auto& s=*state_;
  if(const auto* trace=std::getenv("OCTARYN_CLIENT_SCENE_CONTINUITY"))s.trace=std::strcmp(trace,"1")==0;
  if(s.loaded || (!overlay && (renderer.map || !renderer.resident_maps.empty())) || !renderer.capabilities.virtual_geometry()) {
    s.error="scene residency initialization is invalid";return false;
  }
  if(const char* value=std::getenv("OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB")) {
    unsigned mib{};const auto* end=value+std::strlen(value);const auto parsed=std::from_chars(value,end,mib);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || mib<64 || mib>32768) {
      s.error="scene GPU budget must be 64..32768 MiB";return false;
    }
    gpu_budget=std::uint64_t(mib)*1024*1024;
  }
  renderer.scene_memory=std::make_shared<virtual_geometry::SceneMemoryLedger>(gpu_budget);
  if(!world_ray_adopt_scene_memory(renderer)) {s.error=renderer.status;return false;}
  if(!s.assets.load(renderer,catalog,source,s.error))return false;
  if(s.assets.texture_bytes()>=gpu_budget) {s.error="scene textures exceed residency budget";return false;}
  s.budget=gpu_budget;
  if(const auto* text=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_PIXELS")) {
    char* end{};s.pixel_error=std::strtof(text,&end);
    if(end==text || *end || !std::isfinite(s.pixel_error) || s.pixel_error<0 || s.pixel_error>4) {
      s.error="virtual geometry error must be 0..4 pixels";return false;
    }
  }
  s.overlay=overlay;
  if(!overlay) {
    s.collision=std::make_unique<character_motion::SceneCollisionResidency>();
    if(!s.collision->load(catalog,source,catalog.parent_path()/"collision-scratch")) {s.error=s.collision->error();return false;}
  }
  s.scheduler=octaryn_native_schedule_runtime_create(1,1);
  if(!s.scheduler) {s.error="scene preparation scheduler creation failed";return false;}
  s.loaded=true;
  std::printf("scene_residency_initialized parts=%zu instances=%zu gpu_budget=%llu unique_geometry=1 source_complete=1\n",
      s.assets.parts().size(),s.assets.nodes().size(),static_cast<unsigned long long>(gpu_budget));
  return true;
}
bool SceneSession::State::collect_retired() {
  std::erase_if(retired,[&](const auto& item) {
    if(!map_ray_retirement_ready(*item.map) || (item.map->geometry && !item.map->geometry->stream().gpu_idle()))return false;
    if(item.map.use_count()!=1)return false;
    std::uint64_t value{};
    if(item.fence && (SLANG_FAILED(item.fence->getCurrentValue(&value)) || value==UINT64_MAX)) {
      error="scene retirement fence failed";return false;
    }
    if(value<item.signal)return false;
    if(trace)std::printf("scene_part_collected frame=%llu part=%u fence_signal=%llu fence_completed=%llu ray_idle=1 upload_idle=1\n",
        static_cast<unsigned long long>(render_frame),item.part,static_cast<unsigned long long>(item.signal),
        static_cast<unsigned long long>(value));
    return true;
  });
  retired_bytes=assets.context().ledger->stats().phase_bytes[unsigned(virtual_geometry::SceneMemoryPhase::Retired)];
  return error.empty();
}
void SceneSession::State::retire(std::map<std::uint32_t,Entry>::iterator item) {
  changed=changed || item->second.published;
  if(trace)std::printf("scene_part_retired frame=%llu part=%u fence_signal=%llu published=%u\n",
      static_cast<unsigned long long>(render_frame),item->first,static_cast<unsigned long long>(last_signal),unsigned(item->second.published));
  retired.push_back({std::move(item->second.map),last_fence,last_signal,assets.parts()[item->first].reservation_bytes,item->first});
  entries.erase(item);
}
bool SceneSession::State::publish(WorldRenderer& renderer) {
  if(!changed)return true;
  renderer.scene_resident_maps.clear();
  for(const auto& [id,entry]:entries)if(entry.published && !entry.map->geometry_instances.empty())renderer.scene_resident_maps.push_back(entry.map);
  publish_resident_maps(renderer);
  renderer.scene_changes.notify_column(0,0,0,0,SceneChangeKind::Modified);
  ++generation;changed=false;return true;
}
bool SceneSession::pump(WorldRenderer& renderer,const WorldCamera& camera,const WorldCamera& actor,rhi::ICommandEncoder* commands) {
  auto& s=*state_;if(!s.loaded || !s.error.empty())return false;
  ++s.frame;s.render_frame=renderer.frames;if(!s.collect_retired())return false;
  if(!s.update_cut(renderer,camera))return false;
  auto selected=s.plan.wanted;
  if(s.startup_committed)for(const auto id:s.pending)selected.push_back(s.assets.selection(id));
  if(!s.publish(renderer) || !s.collect_retired() || !s.progress(renderer,selected) ||
      !s.stage(renderer,camera,commands) || !s.publish(renderer))return false;
  if(s.collision && !s.collision->ready(actor.x,actor.y,actor.z,3) && !s.collision->error().empty()) {
    s.error=s.collision->error();return false;
  }
  if(s.frame==1 || s.frame%120==0) {
    std::printf("scene_stream frame=%llu wanted=%zu resident=%zu pending=%zu retired=%zu reservation_bytes=%llu retired_bytes=%llu generation=%llu camera_x=%.3f\n",
        static_cast<unsigned long long>(s.frame),s.plan.wanted.size(),renderer.scene_resident_maps.size(),
        s.entries.size()-renderer.scene_resident_maps.size()+unsigned(s.job.task!=nullptr),s.retired.size(),
        static_cast<unsigned long long>(s.plan.reservation_bytes),static_cast<unsigned long long>(s.retired_bytes),
        static_cast<unsigned long long>(s.generation),camera.x);
  }
  return true;
}
bool SceneSession::capture_ready() const {
  const auto& s=*state_;if(!s.loaded || !s.startup_committed || !s.error.empty() || !s.plan.admitted || s.plan.wanted.empty())return false;
  for(const auto& wanted:s.plan.wanted) {
    const auto found=s.entries.find(wanted.part);
    if(found==s.entries.end() || !found->second.published || !covers(found->second.selected,wanted) ||
        !found->second.map->geometry->ready())return false;
  }
  return true;
}
TileStartupReadiness SceneSession::startup_readiness() const {
  const auto& s=*state_;TileStartupReadiness out{};out.generation=s.generation;
  out.total=unsigned(s.assets.roots().size());out.requested=out.visible=unsigned(s.plan.wanted.size());
  out.requested_set_hash=1469598103934665603ull;
  for(const auto& wanted:s.plan.wanted) {
    out.requested_set_hash=(out.requested_set_hash^wanted.part)*1099511628211ull;
    for(const auto instance:wanted.instances)out.requested_set_hash=(out.requested_set_hash^instance)*1099511628211ull;
    const auto found=s.entries.find(wanted.part);
    if(found!=s.entries.end() && found->second.published && covers(found->second.selected,wanted) &&
        found->second.map->geometry->ready())++out.resident;
  }
  out.visible_missing=out.requested-out.resident;out.requested_ready=capture_ready();
  std::vector<std::uint64_t> mesh_nodes(s.assets.catalog().mesh_count);
  for(const auto& node:s.assets.nodes())++mesh_nodes[node.mesh];
  std::uint64_t complete_pairs{},resident_pairs{};bool all_roots=true;
  for(const auto root:s.assets.roots()) {
    const auto& part=s.assets.parts()[root];complete_pairs+=part.triangle_count*mesh_nodes[part.mesh];
  }
  for(const auto& [id,entry]:s.entries)if(entry.published) {
    resident_pairs+=s.assets.parts()[id].triangle_count*entry.selected.instances.size();all_roots=all_roots && entry.map->geometry->ready();
  }
  // Only exact original-node coverage can certify arbitrary offscreen dependencies.
  out.all_manifest_ready=out.requested_ready && all_roots && complete_pairs && resident_pairs==complete_pairs;
  return out;
}
bool SceneSession::collision_ready(float x,float y,float z,float radius) {
  auto& s=*state_;return s.collision && s.error.empty() && s.collision->ready(x,y,z,radius);
}
std::shared_ptr<character_motion::MeshCollisionScene> SceneSession::collision_scene() const {
  return state_->collision?state_->collision->scene():nullptr;
}
std::uint64_t SceneSession::generation() const {return state_->generation;}
const std::string& SceneSession::error() const {return state_->error;}
}
