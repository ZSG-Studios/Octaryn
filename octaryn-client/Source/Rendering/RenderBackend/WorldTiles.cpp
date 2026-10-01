#include "WorldRendererInternal.h"
#include "WorldStartupReadiness.h"
#include "MapVisibility.h"
#include "../../MapWorld/MapRendererInternal.h"
#include "../../VirtualGeometry/WorldGeometryRay.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
WorldStartupReadiness open_world_renderer_startup_readiness(const WorldRenderer* r,const WorldCamera& camera) {
  WorldStartupReadiness out;if(!r || r->render_width()<=0 || r->render_height()<=0)return out;
  out.ray_required=r->ray_requested;
  if(r->scene_session) {
    out.tiles=r->scene_session->startup_readiness();
  } else if(r->tile_session) {
    auto expanded=camera;
    // Bound temporal subpixel jitter so an edge tile cannot be falsely omitted.
    expanded.jitter_x=std::max(std::abs(camera.jitter_x),2.f/static_cast<float>(r->render_width()));
    expanded.jitter_y=std::max(std::abs(camera.jitter_y),2.f/static_cast<float>(r->render_height()));
    out.tiles=r->tile_session->startup_readiness(map_visibility_camera(expanded,*r));
  } else if(r->map) {
    out.tiles.total=out.tiles.requested=out.tiles.resident=out.tiles.visible=1;
    out.tiles.generation=1;out.tiles.requested_set_hash=1;
    out.tiles.requested_ready=out.tiles.all_manifest_ready=true;
    if(!r->map->geometry || !r->map->geometry->ready()) {
      out.tiles.resident=0;out.tiles.requested_ready=out.tiles.all_manifest_ready=false;
    }
  }
  for(const auto& map:r->resident_maps)if(!map->geometry || !map->geometry->ready()) {
    out.tiles.requested_ready=false;out.tiles.all_manifest_ready=false;
  }
  out.ray_ready=!out.ray_required;
  if(out.ray_required && r->ray_enabled && world_ray_available(*r)) {
    const auto ray=world_ray_stats(*r);
    out.ray_ready=!ray.pending_columns && !ray.active_jobs && world_ray_coverage_complete(*r);
    for(const auto& map:r->resident_maps)out.ray_ready=out.ray_ready && map_ray_ready(*map);
  }
  // No bounded offscreen reflection/shadow dependency set exists yet.
  out.ray_guard_complete=!out.ray_required || out.tiles.all_manifest_ready;
  return out;
}
void refresh_resident_texture_bytes(WorldRenderer& r) {
  std::vector<MapRenderer*> maps;maps.reserve(r.resident_maps.size());
  for(const auto& map:r.resident_maps)maps.push_back(map.get());
  r.resident_texture_bytes=map_unique_texture_bytes(maps);
}
bool open_world_renderer_load_tiles(WorldRenderer* r,const char* manifest,float load_radius,float keep_radius) {
  if(!r || !manifest || !*manifest || r->map || r->tile_session || r->scene_session)return false;
  if(!r->capabilities.virtual_geometry()) {
    r->status="virtual_geometry_hardware_unsupported";return false;
  }
  auto session=std::make_unique<TileSession>();
  world_renderer_load_stage(*r,"Reading world regions");
  TileStreamBudget budget;budget.load_radius=load_radius;budget.keep_radius=keep_radius;
  if(!session->load(std::filesystem::path(reinterpret_cast<const char8_t*>(manifest)),r->device,r->queue,budget)) {
    r->status=session->error();return false;
  }
  r->tile_session=std::move(session);r->status="tiles_loading";return true;
}
void open_world_renderer_set_tile_anchor(WorldRenderer* r,const WorldCamera& anchor) {
  if(r) {r->tile_anchor=anchor;r->tile_anchor_valid=true;}
}
bool open_world_renderer_tile_collision_ready(const WorldRenderer* r,float x,float y,float z,float radius) {
  if(r && r->scene_session)return r->scene_session->collision_ready(x,y,z,radius);
  return r && (!r->tile_session || r->tile_session->collision_ready(x,y,z,radius));
}
std::shared_ptr<character_motion::MeshCollisionScene> open_world_renderer_tile_collision(const WorldRenderer* r) {
  if(r && r->scene_session)return r->scene_session->collision_scene();
  return r && r->tile_session?r->tile_session->collision_scene():nullptr;
}
bool open_world_renderer_prepare_tiles(WorldRenderer* r,const WorldCamera& camera) {
  if(r && r->scene_session) {
    if(!r->scene_session->pump(*r,camera,r->tile_anchor_valid?r->tile_anchor:camera,nullptr)) {
      r->status=r->scene_session->error();return false;
    }
    for(const auto& map:r->resident_maps)
      if(!virtual_geometry::prepare_geometry_ray(*r,*map,camera))return false;
    return true;
  }
  if(!r || !r->tile_session)return false;
  auto commands=r->queue->createCommandEncoder();if(!commands)return false;
  const auto generation=r->tile_session->stats().generation;
  if(!r->tile_session->pump(camera,r->tile_anchor_valid?r->tile_anchor:camera,commands,
      false,r->resident_maps)) {
    r->status=r->tile_session->error();return false;
  }
  r->map=r->resident_maps.empty()?nullptr:r->resident_maps.front().get();
  for(const auto& map:r->resident_maps) {
    if(!map->geometry) {
      map->geometry=std::make_shared<virtual_geometry::WorldGeometry>();
      if(!map->geometry->initialize(*r,*map)) {r->status=map->geometry->error();return false;}
    }
    if(!virtual_geometry::prepare_geometry_ray(*r,*map,camera))return false;
  }

  if(generation!=r->tile_session->stats().generation) {
    refresh_resident_texture_bytes(*r);
    r->scene_changes.notify_column(0,0,0,0,SceneChangeKind::Modified);
  }
  // CPU preparation, fence polling and independently submitted BLAS work do
  // not populate this encoder. Do not submit/wait for an empty startup buffer.
  if(!r->tile_session->commands_recorded())return true;
  auto submission=commands->finish();if(!submission)return false;
  if(SLANG_FAILED(r->queue->submit(submission)) ||
      !r->frame_queue.synchronize(r->queue,frame_fence_timeout_ms()))return false;
  r->tile_session->submitted(r->frame_queue.fence(),r->frame_queue.last_signal());
  return true;
}
}
