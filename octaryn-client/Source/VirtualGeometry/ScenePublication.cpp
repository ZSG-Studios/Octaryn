#include "SceneSessionInternal.h"
#include "WorldGeometry.h"
#include "WorldGeometryRay.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <cstdio>

namespace octaryn::client::rendering {
bool SceneSession::State::stage(WorldRenderer& renderer,const WorldCamera& camera,rhi::ICommandEncoder* commands) {
  // Collision startup owns no frame encoder. Geometry progresses on the normal
  // frame timeline after the startup worker returns ownership to rendering.
  if(!commands)return true;
  const bool ray_required=renderer.ray_requested && world_ray_available(renderer);
  for(auto& [part,entry]:entries)if(!entry.published && !entry.ready) {
    auto& map=*entry.map;
    if(!virtual_geometry::prepare_geometry_ray(renderer,map,camera,false)) {error=renderer.status;return false;}
    if(map.geometry_ray && map.geometry_ray->memory_blocked()) {
      if(!startup_committed) {error="complete coarse scene ray cut exceeds aggregate GPU budget";return false;}
      retry_after[replacing]=frame+120;
      for(const auto id:pending)if(auto candidate=entries.find(id);candidate!=entries.end())retire(candidate);
      pending.clear();removed.clear();replacing=virtual_geometry::invalid_id;
      return true;
    }
    const bool roots=map.geometry->ready(),rays=!ray_required || map_ray_ready(map);
    // Promotion precedes recording: a staged stream cannot also record raster
    // work in the same frame. Only previously completed uploads can be ready.
    if(roots && rays) {
      entry.ready=true;
      if(trace)std::printf("scene_part_published frame=%llu part=%u instances=%zu roots_ready=1 rays_ready=1 generation=%llu ray_required=%u\n",
          static_cast<unsigned long long>(renderer.frames),part,entry.selected.instances.size(),
          static_cast<unsigned long long>(generation+1),unsigned(ray_required));
    } else {
      if(!map.geometry->stage_uploads(commands)) {error=map.geometry->error();return false;}
      if(trace)std::printf("scene_part_staging frame=%llu part=%u roots_ready=%u rays_ready=%u\n",
          static_cast<unsigned long long>(renderer.frames),part,unsigned(roots),unsigned(rays));
    }
  }
  return commit_cut(renderer);
}
bool SceneSession::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;s.last_fence=fence;s.last_signal=value;
  for(const auto& [part,entry]:s.entries)if(!entry.published && !entry.map->geometry->submitted(fence,value)) {
    s.error=entry.map->geometry->error();return false;
  }
  return true;
}
void SceneSession::trace_frame(const WorldRenderer& renderer) const {
  const auto& s=*state_;if(!s.trace)return;
  const bool ray_required=renderer.ray_requested && world_ray_available(renderer);
  unsigned pending{},roots{},rays{},resident{};std::uint64_t allocated{};
  for(const auto& [part,entry]:s.entries) {
    allocated+=s.assets.parts()[part].reservation_bytes;
    if(entry.published) {++resident;continue;}
    ++pending;roots+=entry.map->geometry->ready();rays+=!ray_required || map_ray_ready(*entry.map);
  }
  std::uint64_t completed{};
  if(s.last_fence && SLANG_FAILED(s.last_fence->getCurrentValue(&completed)))completed=UINT64_MAX;
  const auto ray=world_ray_stats(renderer);
  std::printf("scene_continuity frame=%llu generation=%llu resident=%u pending=%u cpu_pending=%u pending_roots=%u pending_rays=%u "
      "ray_requested=%u ray_enabled=%u coverage_complete=%u reflection_valid=%u ray_generation=%llu "
      "reservation_bytes=%llu allocated_reservation_bytes=%llu retired_bytes=%llu budget=%llu retired=%zu fence_signal=%llu fence_completed=%llu\n",
      static_cast<unsigned long long>(renderer.frames),static_cast<unsigned long long>(s.generation),resident,pending,
      unsigned(s.job.task!=nullptr),roots,rays,unsigned(renderer.ray_requested),unsigned(renderer.ray_enabled),
      unsigned(world_ray_coverage_complete(renderer)),unsigned(renderer.map_reflections.valid),
      static_cast<unsigned long long>(ray.scene_generation),static_cast<unsigned long long>(s.plan.reservation_bytes),
      static_cast<unsigned long long>(allocated),
      static_cast<unsigned long long>(s.retired_bytes),static_cast<unsigned long long>(s.budget),s.retired.size(),
      static_cast<unsigned long long>(s.last_signal),static_cast<unsigned long long>(completed));
}
}
