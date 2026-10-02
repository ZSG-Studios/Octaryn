#include "WorldRendererInternal.h"
#include "WorldStartupReadiness.h"
#include "MapVisibility.h"
#include "TileActorAnchor.h"
#include "../../MapWorld/MapRendererInternal.h"
#include "../../VirtualGeometry/WorldGeometryRay.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
WorldStartupReadiness open_world_renderer_startup_readiness(const WorldRenderer* r,const WorldCamera& camera) {
  WorldStartupReadiness out;if(!r || r->render_width()<=0 || r->render_height()<=0)return out;
  out.ray_required=r->ray_requested;
  if(r->scene_session && !r->tile_session) {
    out.tiles=r->scene_session->startup_readiness();
  } else if(r->tile_session) {
    auto expanded=camera;
    // Bound temporal subpixel jitter so an edge tile cannot be falsely omitted.
    expanded.jitter_x=std::max(std::abs(camera.jitter_x),2.f/static_cast<float>(r->render_width()));
    expanded.jitter_y=std::max(std::abs(camera.jitter_y),2.f/static_cast<float>(r->render_height()));
    out.tiles=r->tile_session->startup_readiness(map_visibility_camera(expanded,*r));
    if(r->scene_session) {
      const auto overlay=r->scene_session->startup_readiness();
      out.tiles.total+=overlay.total;out.tiles.requested+=overlay.requested;
      out.tiles.resident+=overlay.resident;out.tiles.visible+=overlay.visible;
      out.tiles.visible_missing+=overlay.visible_missing;
      out.tiles.requested_ready=out.tiles.requested_ready && overlay.requested_ready;
      out.tiles.all_manifest_ready=out.tiles.all_manifest_ready && overlay.all_manifest_ready;
      out.tiles.generation=(out.tiles.generation<<32)^overlay.generation;
      out.tiles.requested_set_hash^=overlay.requested_set_hash;
    }
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
void publish_resident_maps(WorldRenderer& r) {
  r.resident_maps=r.scene_resident_maps;
  r.resident_maps.insert(r.resident_maps.end(),r.tile_resident_maps.begin(),r.tile_resident_maps.end());
  r.map=r.resident_maps.empty()?nullptr:r.resident_maps.front().get();
  refresh_resident_texture_bytes(r);
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
  r->tile_session=std::move(session);
  if(!load_scene_physics_overlay(*r,std::filesystem::path(reinterpret_cast<const char8_t*>(manifest))))return false;
  r->status="tiles_loading";return true;
}
bool open_world_renderer_set_desired_regions(WorldRenderer* r,std::span<const std::uint32_t> wanted,std::span<const std::uint32_t> retained) {
 return r && r->tile_session && r->tile_session->set_desired_regions(wanted,retained);
}
bool open_world_renderer_region_count(const WorldRenderer* r,std::uint32_t& count,std::uint64_t& generation) {
 count=0;generation=0;return r && r->tile_session && r->tile_session->region_count(count,generation);
}
bool open_world_renderer_region_status(const WorldRenderer* r,std::uint32_t index,octaryn_host_region_status& out) {
 return r && r->tile_session && r->tile_session->region_status(index,out);
}
bool open_world_renderer_actor_position(const WorldRenderer* r,octaryn_host_region_anchor& out) {
 out={};return r && tile_actor_anchor(bool(r->tile_session),r->tile_anchor_valid,r->tile_anchor_authoritative,
     r->tile_anchor.x,r->tile_anchor.y,r->tile_anchor.z,out);
}
void open_world_renderer_set_tile_anchor(WorldRenderer* r,const WorldCamera& anchor,bool authoritative) {
  if(r) {r->tile_anchor=anchor;r->tile_anchor_valid=true;r->tile_anchor_authoritative=authoritative;}
}
bool open_world_renderer_tile_collision_ready(const WorldRenderer* r,float x,float y,float z,float radius) {
  if(r && r->scene_session && !r->tile_session)return r->scene_session->collision_ready(x,y,z,radius);
  return r && (!r->tile_session || r->tile_session->collision_ready(x,y,z,radius));
}
std::shared_ptr<character_motion::MeshCollisionScene> open_world_renderer_tile_collision(const WorldRenderer* r) {
  if(r && r->scene_session && !r->tile_session)return r->scene_session->collision_scene();
  return r && r->tile_session?r->tile_session->collision_scene():nullptr;
}
bool open_world_renderer_prepare_tiles(WorldRenderer* r,const WorldCamera& camera) {
  if(r && r->scene_session) {
    if(!r->scene_session->pump(*r,camera,r->tile_anchor_valid?r->tile_anchor:camera,nullptr)) {
      r->status=r->scene_session->error();return false;
    }
    if(!r->tile_session) {
      for(const auto& map:r->resident_maps)
        if(!virtual_geometry::prepare_geometry_ray(*r,*map,camera))return false;
      return true;
    }
  }
  if(!r || !r->tile_session)return false;
  auto commands=r->queue->createCommandEncoder();if(!commands)return false;
  const auto generation=r->tile_session->stats().generation;
  if(!r->tile_session->pump(camera,r->tile_anchor_valid?r->tile_anchor:camera,commands,
      false,r->tile_resident_maps)) {
    r->status=r->tile_session->error();return false;
  }
  if(generation!=r->tile_session->stats().generation)publish_resident_maps(*r);
  for(const auto& map:r->resident_maps) {
    if(!map->geometry) {
      map->geometry=std::make_shared<virtual_geometry::WorldGeometry>();
      if(!map->geometry->initialize(*r,*map)) {r->status=map->geometry->error();return false;}
    }
    if(!virtual_geometry::prepare_geometry_ray(*r,*map,camera))return false;
  }

  if(generation!=r->tile_session->stats().generation) {
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
