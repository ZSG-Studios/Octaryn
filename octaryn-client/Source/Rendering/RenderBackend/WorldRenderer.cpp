#include "WorldRendererInternal.h"
#include "WorldMeshInput.h"
#include "WorldStream.h"
#include "Camera.h"
#include "LightingSystem.h"
#include "FrameWatchdog.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <memory>
#include "WorldFrame.h"
#include "../BlockTransportGI/DynamicReceivers.h"
namespace octaryn::client::rendering {
WorldRenderer* open_world_renderer_create(SDL_Window* window, WorldBootProgressFn progress, void* progress_user, WorldBootMainFn main_thread) {
  if (!window) return nullptr;
  const auto cleanup=[main_thread,progress_user](WorldRenderer* renderer) {
    const auto destroy=[](void* argument) {delete static_cast<WorldRenderer*>(argument);};
    if(main_thread)main_thread(destroy,renderer,progress_user);
    else destroy(renderer);
  };
  std::unique_ptr<WorldRenderer,decltype(cleanup)> renderer(new WorldRenderer,cleanup);
  renderer->window=window;
  renderer->culling_enabled=SDL_getenv("OCTARYN_CLIENT_DISABLE_CULLING")==nullptr;
  struct BootProgress {
    WorldRenderer* renderer;
    WorldBootProgressFn progress;
    void* user;
    WorldBootMainFn main_thread;
    static void report(const char* stage,void* argument) {
      auto& boot=*static_cast<BootProgress*>(argument);
      if(boot.progress)boot.progress(stage,boot.user);
      if(!boot.renderer->ui_renderer)return;
      struct Frame {WorldRenderer* renderer;const char* stage;bool ready{};} frame{boot.renderer,stage};
      const auto draw=[](void* argument) {
        auto& frame=*static_cast<Frame*>(argument);
        frame.ready=world_renderer_boot_frame(*frame.renderer,frame.stage);
      };
      if(boot.main_thread)boot.main_thread(draw,&frame,boot.user);
      else draw(&frame);
      if(!frame.ready)throw std::runtime_error("Startup loading frame failed");
    }
    static void dispatch(void (*operation)(void*),void* data,void* argument) {
      auto& boot=*static_cast<BootProgress*>(argument);
      if(boot.main_thread)boot.main_thread(operation,data,boot.user);
      else operation(data);
    }
  } boot{renderer.get(),progress,progress_user,main_thread};
  if (!world_renderer_create_device(*renderer, BootProgress::report, &boot, BootProgress::dispatch)) {
    std::fprintf(stderr,"world_renderer_initialize_failed stage=%s sdl_error=%s\n",
        renderer->status.c_str(),SDL_GetError());
    return nullptr;
  }
  if(const auto* path=SDL_getenv("OCTARYN_CLIENT_GPU_PROFILE_PATH");path && *path)
    renderer->gpu_profile=std::make_unique<WorldGpuProfile>(renderer->device,path);
  open_world_renderer_set_scene(renderer.get(),{});
  renderer->status="ready";
  return renderer.release();
}
void open_world_renderer_set_present(WorldRenderer* r,int present_mode) {
  if(!r)return;
  present_mode=std::clamp(present_mode,0,2);
  if(r->present_mode==present_mode && !r->present_dirty)return;
  r->present_mode=present_mode;
  r->present_dirty=true;
}
void open_world_renderer_set_scene(WorldRenderer* r,const WorldSceneSettings& settings) {
  if(r)configure_temporal(r->temporal,settings,SDL_getenv("OCTARYN_CLIENT_UPSCALER")==nullptr);
  if (!r) return;
  r->sky=make_sky_uniforms(settings.day_fraction,settings.seconds,
      {settings.gradient,settings.stars,settings.sun,settings.moon});
  r->lighting=make_sky_lighting(settings.day_fraction,r->lighting_config);
  r->clouds=settings.clouds;r->fog_distance=settings.fog?std::clamp(settings.fog_distance,64.f,2048.f):0;
  r->pbr=settings.pbr; r->pom=settings.pom;
  const auto* ray_mode=SDL_getenv("OCTARYN_CLIENT_RAY_TRACING");
  r->ray_effects=settings.ray_tracing || (ray_mode && std::string_view(ray_mode)=="required");
  r->ray_requested=r->gi_mode==GiMode::BlockTransport || r->ray_effects;
  r->ray_enabled=r->ray_requested && (!r->map || map_ray_ready(*r->map));
}
void open_world_renderer_set_selection(WorldRenderer* r,const SelectionTarget& target) {if(r) r->selection=target;}
void open_world_renderer_set_player(WorldRenderer* r,const PlayerPose& pose) {if(r) r->player_pose=pose;}
void open_world_renderer_set_items(WorldRenderer* r,std::shared_ptr<const world_presentation::WorldItemSnapshot> items) {
  if(r)r->item_snapshot=std::move(items);
}
void open_world_renderer_set_capture_enabled(WorldRenderer* r,bool enabled) {if(r)r->capture_enabled=enabled;}
bool open_world_renderer_captured(const WorldRenderer* r) {return r && r->captured;}
Rml::RenderInterface* open_world_renderer_ui_interface(WorldRenderer* r) {return r?rml_render_interface(r->ui_renderer):nullptr;}
void open_world_renderer_set_ui_context(WorldRenderer* r,Rml::Context* context) {if(r) r->ui_context=context;}
unsigned open_world_renderer_ui_tile(WorldRenderer* r,std::uint16_t block) {return r?world_atlas_preview_layer(r->atlas,block):0;}
void open_world_renderer_set_lighting(WorldRenderer* r,const lighting_settings& settings) {if(r) r->lighting_config=settings;}
namespace {
std::uint64_t column_bytes(const WorldColumnGpu& column) {
  std::uint64_t patches{};
  for(const auto count:column.patch_counts) patches+=count;
  return std::max(1u,column.face_count)*16ull+std::max(std::uint64_t{1},patches)*4+
      std::max(1u,column.pass_counts[3]+column.pass_counts[4])*32ull;
}
}
void world_renderer_store_column(WorldRenderer& r,std::pair<std::int32_t,std::int32_t> coordinate,WorldColumnGpu column) {
  const auto previous=r.columns.find(coordinate);
  const auto old_quads=previous==r.columns.end()?0:previous->second.face_count;
  const auto old_bytes=previous==r.columns.end()?0:column_bytes(previous->second);
  const auto new_quads=column.face_count;
  const auto new_bytes=column_bytes(column);
  const auto change_kind=previous==r.columns.end()?SceneChangeKind::Added:SceneChangeKind::Modified;
  const int changed_min_y=column.min_y,changed_height=column.height;
  // Commit accounting only after the map accepts the new/replacement column.
  r.columns.insert_or_assign(coordinate,std::move(column));
  const auto source=r.sources.find(coordinate);
  if(source!=r.sources.end())world_block_lights_store(r,source->second);
  r.scene_changes.notify_column(coordinate.first,coordinate.second,changed_min_y,changed_height,change_kind);
  r.resident_quads=r.resident_quads-old_quads+new_quads;
  r.column_gpu_bytes=r.column_gpu_bytes-old_bytes+new_bytes;
  r.dirty.erase(coordinate);r.dirty_urgent.erase(coordinate);
}
bool open_world_renderer_update(WorldRenderer* r,const world_presentation::StreamColumn& column) {
  if (!r) return false;
  if (std::abs(std::int64_t(column.x)-r->center_x)>r->radius ||
      std::abs(std::int64_t(column.z)-r->center_z)>r->radius) return true;
  world_mesh_invalidate_neighbors(*r,column);
  r->sources.insert_or_assign({column.x,column.z},column);
  world_renderer_reapply_predicted_edits(*r,{column.x,column.z});
  WorldColumnGpu gpu;
  if (!world_renderer_mesh(*r,r->sources.at({column.x,column.z}),gpu)) return false;
  r->sources.at({column.x,column.z}).mesh_halo.reset();
  const auto base=r->prediction_bases.find({column.x,column.z});
  if(base!=r->prediction_bases.end())base->second.mesh_halo.reset();
  gpu.min_y=column.min_y; gpu.height=column.height;
  world_renderer_store_column(*r,{column.x,column.z},std::move(gpu));
  r->dirty.erase({column.x,column.z});
  r->status="terrain_resident";
  return true;
}
bool open_world_renderer_render_menu(WorldRenderer* r) {
  if (!r) return false;
  int width{},height{};
  SDL_GetWindowSizeInPixels(r->window,&width,&height);
  if (width<=0 || height<=0 || (SDL_GetWindowFlags(r->window)&SDL_WINDOW_MINIMIZED)) return true;
  if ((r->present_dirty || width!=r->width || height!=r->height) && !world_renderer_resize(*r,width,height)) return false;
  r->active_frame=r->frame_queue.slot(r->frames);
  if(!r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms()))return false;
  Slang::ComPtr<rhi::ITexture> image;
  if(!world_rhi_ok(r->surface->acquireNextImage(image.writeRef()))) return false;
  if(!image) return world_renderer_resize(*r,r->width,r->height);
  auto commands=r->queue->createCommandEncoder();
  if(!commands) return false;
  float black[4]{};
  commands->clearTextureFloat(r->target().color,{0,1,0,1},black);
  if(!render_rml(r->ui_renderer,commands,r->target().color_view,r->ui_context,r->width,r->height))return false;
  // Standalone RHI tracks all attachment, shader, copy and present transitions.
  const rhi::SubresourceRange copy_range{0,1,0,1};
  commands->copyTexture(image,copy_range,{},r->target().color,copy_range,{},
      {static_cast<std::uint32_t>(r->width),static_cast<std::uint32_t>(r->height),1});
  commands->setTextureState(image,rhi::ResourceState::Present);
  auto submission=commands->finish();
  if(!submission) return false;
  if(!r->frame_queue.submit(r->queue,submission,r->active_frame))return false;
  if(!world_rhi_ok(r->surface->present())) return false;
  if(r->frame_queue.count()==1 && !r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms()))return false;
  r->status="menu_presented";
  ++r->frames;return true;
}
bool open_world_renderer_render(WorldRenderer* r,const WorldCamera& camera) {
  if (!r) return false;
  int width{},height{};
  SDL_GetWindowSizeInPixels(r->window,&width,&height);
  if (width<=0 || height<=0 || (SDL_GetWindowFlags(r->window)&SDL_WINDOW_MINIMIZED)) return true;
  const bool mode_changed=r->temporal.requested_mode!=r->temporal.mode;
  if ((mode_changed || r->temporal.reconfigure || r->present_dirty || width!=r->width || height!=r->height) && !world_renderer_resize(*r,width,height)) return false;
  if (!render_world_frame(*r,camera)) {
    std::fprintf(stderr,"world_frame_failed stage=%s frames=%llu\n",r->frame_fail_stage,(unsigned long long)r->frames);
    if(r->status!="fence_timeout" && r->status!="frame_watchdog")r->status="world_frame_failed";
    return false;
  }
  r->status="world_presented";
  return true;
}
void open_world_renderer_set_center(WorldRenderer* r,std::int32_t x,std::int32_t z,int radius) {
  if (!r) return;
  const auto clamped_radius=std::clamp(radius,0,32);
  if(r->center_x==x && r->center_z==z && r->radius==clamped_radius) return;
  r->center_x=x; r->center_z=z; r->radius=clamped_radius;
  if(r->delivery_jobs)r->delivery_jobs->retain_window(*r);
  for (auto it=r->columns.begin();it!=r->columns.end();) {
    if (std::abs(std::int64_t(it->first.first)-x)>r->radius ||
        std::abs(std::int64_t(it->first.second)-z)>r->radius) {
      r->resident_quads-=it->second.face_count;
      r->scene_changes.notify_column(it->first.first,it->first.second,it->second.min_y,it->second.height,SceneChangeKind::Removed);
      r->column_gpu_bytes-=column_bytes(it->second);
      const auto retired=it->first;world_block_lights_remove(*r,retired);
      r->sources.erase(retired);r->prediction_bases.erase(retired);r->dirty.erase(retired);r->dirty_urgent.erase(retired);it=r->columns.erase(it);
      for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
        const auto neighbor=std::make_pair(retired.first+dx,retired.second+dz);
        if(r->sources.contains(neighbor)) {r->dirty.insert(neighbor);r->dirty_urgent.insert(neighbor);}
      }
    }
    else ++it;
  }
  world_mesh_invalidate_preloaded_outside_window(*r);
}
WorldRendererStats open_world_renderer_stats(const WorldRenderer* r) {
  WorldRendererStats stats{};
  if (!r) return stats;
  stats.columns=static_cast<std::uint32_t>(r->columns.size()); stats.frames=r->frames;
  stats.drawn_columns=r->drawn_columns; stats.drawn_quads=r->drawn_quads;
  stats.quads=r->resident_quads;
  stats.pending_meshes=static_cast<std::uint32_t>(r->dirty.size()+(r->halo_jobs?r->halo_jobs->pending():0)+(r->delivery_jobs?r->delivery_jobs->pending():0));
  stats.upscaler_mode=r->temporal.mode;stats.render_width=unsigned(r->render_width());stats.render_height=unsigned(r->render_height());
  stats.temporal_resets=r->temporal.reset_count;
  stats.fsr_dynamic_active=r->temporal.resolution.active;
  stats.fsr_render_scale=r->width?float(r->render_width())/float(r->width):1.f;
  stats.fsr_gpu_ms=r->temporal.resolution.average_ms;
  stats.ray_tracing_available=world_ray_available(*r);
  stats.ray_tracing_active=stats.ray_tracing_available && r->ray_enabled;
  stats.display_width=unsigned(r->width);stats.display_height=unsigned(r->height);
  stats.gpu_bytes=r->column_gpu_bytes+std::uint64_t(r->width)*static_cast<std::uint64_t>(r->height)*52*r->frame_queue.count();
  if(r->temporal.mode) {
    const auto display=std::uint64_t(r->width)*std::uint64_t(r->height);
    const auto render=std::uint64_t(r->temporal.allocation_width)*std::uint64_t(r->temporal.allocation_height);
    stats.gpu_bytes=r->column_gpu_bytes+(render*69+display*12)*r->frame_queue.count()+fsr2_gpu_bytes(r->temporal.fsr);
  }
  if(r->halo_jobs)stats.gpu_bytes+=r->halo_jobs->gpu_bytes();
  if(r->qualification_mesh)stats.gpu_bytes+=r->qualification_mesh->gpu_bytes();
  if(r->delivery_jobs)stats.gpu_bytes+=r->delivery_jobs->gpu_bytes();
  if(r->batch)stats.gpu_bytes+=r->batch->gpu_bytes();
  const auto ray=world_ray_stats(*r);
  stats.ray_ready_columns=ray.ready_columns;
  stats.ray_pending_columns=stats.ray_tracing_active?ray.pending_columns:0;
  stats.map_ready=r->map!=nullptr;
  stats.map_primitives=r->map?static_cast<std::uint32_t>(map_model(*r->map).primitives.size()):0;
  stats.gpu_bytes+=ray.blas_bytes+ray.tlas_bytes+ray.temporary_bytes+ray.retired_mesh_bytes+
      r->local_lighting.gpu_bytes+r->block_gi.stats.total_gpu_bytes;
  if(const auto* dynamic=player_receiver_stats(r->player))stats.gpu_bytes+=dynamic->gpu_bytes;
  if(const auto* dynamic=item_receiver_stats(r->items))stats.gpu_bytes+=dynamic->gpu_bytes;
  stats.gi_ready=r->gi_mode==GiMode::Direct || (r->block_gi.active && r->block_gi.stats.coverage_valid && r->block_gi.stats.transport_ready);
  for(const auto& h:r->rt_shadows.history)for(auto* texture:{h.raw.get(),h.shadow.get(),h.position.get(),h.voxel.get()})
    if(texture)stats.gpu_bytes+=std::uint64_t(r->rt_shadows.width)*r->rt_shadows.height*(texture==h.position.get() || texture==h.shadow.get()?16:4);
  if(r->shadow_fallback.resolution)stats.gpu_bytes+=std::uint64_t(r->shadow_fallback.resolution)*r->shadow_fallback.resolution*12;
  stats.gpu_bytes+=r->shadow_fallback.batch.gpu_bytes();
  stats.gpu_bytes+=std::uint64_t(r->local_shadows.allocated_resolution)*r->local_shadows.allocated_resolution*24;
  return stats;
}
const char* open_world_renderer_status(const WorldRenderer* r) { return r?r->status.c_str():"renderer_unavailable"; }
static bool map_glb_load(WorldRenderer& r, const std::filesystem::path& glb_path) {
  if(r.gi_mode==GiMode::BlockTransport) {r.status="block_transport_requires_voxel_world";return false;}
  if(r.map) {r.status="map_already_loaded";return false;}
  r.status="map_loading";
  const auto utf8=glb_path.generic_u8string();
  auto* map=create_map_renderer(r.device.get(),rhi::Format::RGBA16Float,rhi::Format::D32Float,
      reinterpret_cast<const char*>(utf8.c_str()),"octaryn-client/Shaders/Map/WorldMap.slang");
  if(!map) {r.status="map_load_failed";return false;}
  if(r.ray_requested && world_ray_available(r)) {
    r.status="map_ray_initializing";
    if(!initialize_map_ray_scene(*map,r.queue.get()) || r.debug.errors.load()!=0) {
      destroy_map_renderer(map);r.status="map_ray_initialization_failed";return false;
    }
  }
  r.map=map;
  r.status="map_ready";
  return true;
}
bool open_world_renderer_load_map(WorldRenderer* r, const char* glb_path) {
  if(!r || !glb_path || !*glb_path) {if(r)r->status="map_load_invalid_path";return false;}
  if(!r->queue) {r->status="map_load_no_device";return false;}
  const auto start=std::chrono::steady_clock::now();
  if(!map_glb_load(*r,std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path))))return false;
  const auto& model=map_model(*r->map);
  std::printf("map_model_loaded triangles=%zu primitives=%zu images=%zu ms=%.1f\n",
      model.indices.size()/3,model.primitives.size(),model.images.size(),
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  return true;
}
bool open_world_renderer_map_ready(const WorldRenderer* r) {return r && r->map!=nullptr;}
bool open_world_renderer_flush(WorldRenderer* r) {
  return r && r->frame_queue.drain() && r->lighting_profile.drain() && (!r->gpu_profile || r->gpu_profile->drain()) && r->debug.errors.load()==0;
}
void open_world_renderer_destroy(WorldRenderer* r) { delete r; }
}
