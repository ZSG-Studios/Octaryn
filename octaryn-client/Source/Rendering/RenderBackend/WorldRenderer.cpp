#include "WorldRendererInternal.h"
#include "Camera.h"
#include "LightingSystem.h"
#include "FrameWatchdog.h"
#include "FrameCpuTrace.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <memory>
#include <string_view>
#include "WorldFrame.h"
#include "DynamicReceivers.h"
#include "DeviceMemory.h"
#include "../../MapWorld/MapRendererInternal.h"
#include "../../VirtualGeometry/WorldGeometryRay.h"
#include "../../VirtualGeometry/SceneCatalog.h"
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
  if(r->performance_profile==PerformanceProfile::HQ200)present_mode=0;
  present_mode=std::clamp(present_mode,0,2);
  if(r->present_mode==present_mode && !r->present_dirty)return;
  r->present_mode=present_mode;
  r->present_dirty=true;
}
void open_world_renderer_set_scene(WorldRenderer* r,const WorldSceneSettings& requested) {
  if (!r) return;
  const auto settings=apply_performance_profile(r->performance_profile,requested);
  configure_temporal(r->temporal,settings,r->performance_profile==PerformanceProfile::HQ200 ||
      SDL_getenv("OCTARYN_CLIENT_UPSCALER")==nullptr);
  r->sky=make_sky_uniforms(settings.day_fraction,settings.seconds,
      {settings.gradient,settings.stars,settings.sun,settings.moon});
  r->lighting=make_sky_lighting(settings.day_fraction,r->lighting_config);
  r->clouds=settings.clouds;r->fog_distance=settings.fog?std::clamp(settings.fog_distance,64.f,2048.f):0;
  r->pbr=settings.pbr; r->pom=settings.pom;
  const auto* ray_mode=SDL_getenv("OCTARYN_CLIENT_RAY_TRACING");
  r->ray_effects=settings.ray_tracing || (ray_mode && std::string_view(ray_mode)=="required");
  r->ray_requested=r->ray_effects;
  r->ray_enabled=r->ray_requested && (!r->map || map_ray_ready(*r->map));
}
void open_world_renderer_set_capture_enabled(WorldRenderer* r,bool enabled) {if(r)r->capture_enabled=enabled;}
void open_world_renderer_set_validation_sampling(WorldRenderer* r,bool enabled,std::uint64_t frame) {
  if(!r)return;
  auto& temporal=r->temporal;
  if(temporal.fixed_sampling!=enabled || (enabled && frame<temporal.validation_frame)) {
    temporal.history.invalidate();r->map_reflections.valid=false;r->rt_shadows.valid=false;
  }
  temporal.fixed_sampling=enabled;temporal.validation_frame=frame;
}
bool open_world_renderer_captured(const WorldRenderer* r) {return r && r->captured;}
Rml::RenderInterface* open_world_renderer_ui_interface(WorldRenderer* r) {return r?rml_render_interface(r->ui_renderer):nullptr;}
void open_world_renderer_set_ui_context(WorldRenderer* r,Rml::Context* context) {if(r) r->ui_context=context;}
void open_world_renderer_set_lighting(WorldRenderer* r,const lighting_settings& settings) {if(r) r->lighting_config=settings;}
static bool render_menu_frame(WorldRenderer* r, Rml::Context* context,FrameCpuTrace& trace) {
  trace.begin("menu_resize");
  int width{},height{};
  SDL_GetWindowSizeInPixels(r->window,&width,&height);
  if (width<=0 || height<=0 || (SDL_GetWindowFlags(r->window)&SDL_WINDOW_MINIMIZED)) {trace.finish("skipped");return true;}
  if ((r->present_dirty || width!=r->width || height!=r->height) && !world_renderer_resize(*r,width,height)) return false;
  r->active_frame=r->frame_queue.slot(r->frames);
  trace.begin("menu_frame_fence");
  if(!r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms(),trace.enabled()?&trace:nullptr))return trace.failed();
  trace.begin("menu_acquire");
  Slang::ComPtr<rhi::ITexture> image;
  if(!world_rhi_ok(r->surface->acquireNextImage(image.writeRef()))) return false;
  if(!image) {
    trace.begin("menu_acquire_resize");
    const bool resized=world_renderer_resize(*r,r->width,r->height);
    trace.finish(resized?"abandoned":"failed");return resized;
  }
  trace.begin("menu_rml_encode");
  auto commands=r->queue->createCommandEncoder();
  if(!commands) return false;
  float black[4]{};
  commands->clearTextureFloat(r->target().color,{0,1,0,1},black);
  if(!render_rml(r->ui_renderer,commands,r->target().color_view,context,r->width,r->height))return false;
  // Standalone RHI tracks all attachment, shader, copy and present transitions.
  const rhi::SubresourceRange copy_range{0,1,0,1};
  commands->copyTexture(image,copy_range,{},r->target().color,copy_range,{},
      {static_cast<std::uint32_t>(r->width),static_cast<std::uint32_t>(r->height),1});
  commands->setTextureState(image,rhi::ResourceState::Present);
  trace.begin("menu_finish");
  auto submission=commands->finish();
  if(!submission) return false;
  trace.begin("menu_submit");
  if(!r->frame_queue.submit(r->queue,submission,r->active_frame,r->frames))return trace.failed();
  trace.begin("menu_present");
  if(!world_rhi_ok(r->surface->present())) return false;
  trace.begin("menu_serialized_wait");
  if(r->frame_queue.count()==1 && !r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms(),trace.enabled()?&trace:nullptr))return trace.failed();
  trace.finish();
  r->status="menu_presented";
  ++r->frames;return true;
}
static bool render_menu_context(WorldRenderer* r,Rml::Context* context) {
  if(!r)return false;
  FrameCpuTrace trace(r->frame_cpu,r->frames,"menu");
  const bool rendered=render_menu_frame(r,context,trace);
  if(!rendered)trace.failed();
  trace.begin("menu_return");return trace.complete() && rendered;
}
bool open_world_renderer_render_menu(WorldRenderer* r) {
  return r ? render_menu_context(r, r->ui_context) : false;
}
bool open_world_renderer_render_menu_context(WorldRenderer* r, Rml::Context* context) {
  return r && context ? render_menu_context(r, context) : false;
}
bool open_world_renderer_render(WorldRenderer* r,const WorldCamera& camera) {
  if (!r) return false;
  FrameCpuTrace trace(r->frame_cpu,r->frames,"world");
  trace.begin("window_state");
  int width{},height{};
  SDL_GetWindowSizeInPixels(r->window,&width,&height);
  if (width<=0 || height<=0 || (SDL_GetWindowFlags(r->window)&SDL_WINDOW_MINIMIZED)) {return trace.complete("skipped");}
  trace.begin("renderer_resize");
  const bool mode_changed=r->temporal.requested_mode!=r->temporal.mode;
  if ((mode_changed || r->temporal.reconfigure || r->present_dirty || width!=r->width || height!=r->height) && !world_renderer_resize(*r,width,height)) return false;
  if (!render_world_frame(*r,camera,trace)) {
    r->frame_failed=true;
    bool geometry_error=false;
    for(const auto& map:r->resident_maps) {
      if(map->geometry && !map->geometry->error().empty()) {r->status=map->geometry->error();geometry_error=true;break;}
      if(map->geometry_ray && !map->geometry_ray->error().empty()) {r->status=map->geometry_ray->error();geometry_error=true;break;}
    }
    if(!geometry_error) {
      if(r->scene_session && !r->scene_session->error().empty())r->status=r->scene_session->error();
      else if(r->tile_session && *r->tile_session->error())r->status=r->tile_session->error();
      else if(r->status!="fence_timeout" && r->status!="frame_watchdog")r->status="world_frame_failed";
    }
    std::fprintf(stderr,"world_frame_failed stage=%s frames=%llu reason=%s\n",r->frame_fail_stage,
        (unsigned long long)r->frames,r->status.c_str());
    return trace.failed();
  }
  trace.begin("renderer_return");
  r->status="world_presented";
  return trace.complete();
}
WorldRendererStats open_world_renderer_stats(const WorldRenderer* r) {
  WorldRendererStats stats{};
  if (!r) return stats;
  stats.frames=r->frames;
  stats.upscaler_mode=r->temporal.mode;stats.render_width=unsigned(r->render_width());stats.render_height=unsigned(r->render_height());
  stats.temporal_resets=r->temporal.reset_count;
  stats.fsr_dynamic_active=r->temporal.resolution.active;
  stats.fsr_render_scale=r->width?float(r->render_width())/float(r->width):1.f;
  stats.fsr_gpu_ms=r->temporal.resolution.average_ms;
  stats.ray_tracing_available=world_ray_available(*r);
  stats.ray_tracing_active=stats.ray_tracing_available && r->ray_enabled;
  stats.display_width=unsigned(r->width);stats.display_height=unsigned(r->height);
  stats.gpu_bytes=std::uint64_t(r->width)*static_cast<std::uint64_t>(r->height)*52*r->frame_queue.count();
  if(r->temporal.mode) {
    const auto display=std::uint64_t(r->width)*std::uint64_t(r->height);
    const auto render=std::uint64_t(r->temporal.allocation_width)*std::uint64_t(r->temporal.allocation_height);
    stats.gpu_bytes=(render*69+display*12)*r->frame_queue.count()+fsr2_gpu_bytes(r->temporal.fsr);
  }
  const auto ray=world_ray_stats(*r);
  stats.map_ready=r->map!=nullptr;
  for(const auto& map:r->resident_maps)stats.map_primitives+=static_cast<std::uint32_t>(map_model(*map).primitives.size());
  stats.gpu_bytes+=ray.blas_bytes+ray.tlas_bytes+ray.temporary_bytes+ray.retired_mesh_bytes+
      r->local_lighting.gpu_bytes+r->block_transport_lookup.gpu_bytes;
  stats.gi_ready=true;
  const auto texture_bytes=[](rhi::ITexture* texture) -> std::uint64_t {
    if(!texture)return 0;
    const auto& desc=texture->getDesc();const auto& format=rhi::getFormatInfo(desc.format);
    return std::uint64_t(desc.size.width)*desc.size.height*format.blockSizeInBytes;
  };
  for(const auto& h:r->rt_shadows.history)for(auto* texture:{h.raw.get(),h.shadow.get(),h.position.get(),h.voxel.get()})
    stats.gpu_bytes+=texture_bytes(texture);
  for(const auto& h:r->map_reflections.history)
    for(auto* texture:{h.radiance.texture.get(),h.position.texture.get(),h.surface.texture.get(),h.material.texture.get(),h.moments.texture.get()})
      stats.gpu_bytes+=texture_bytes(texture);
  stats.gpu_bytes+=texture_bytes(r->map_reflections.filtered.texture);
  stats.gpu_bytes+=texture_bytes(r->map_reflections.tiles.texture);
  stats.gpu_bytes+=r->map_reflections.queue.bytes;
  for(const auto& resident:r->resident_maps) {
    const auto map=map_memory_stats(*resident);
    stats.map_geometry_bytes+=map.geometry;
    stats.map_acceleration_bytes+=map.acceleration;stats.map_scratch_bytes+=map.scratch;
    stats.gpu_bytes+=map.geometry+map.acceleration+map.scratch;
  }
  stats.map_texture_bytes=r->resident_texture_bytes;stats.gpu_bytes+=stats.map_texture_bytes;
  stats.gpu_bytes+=r->items.gpu_bytes;
  if(r->geometry_raster)stats.gpu_bytes+=r->geometry_raster->gpu_bytes();
  stats.world_items=static_cast<std::uint32_t>(r->items.poses.size());
  stats.item_assets=static_cast<std::uint32_t>(r->items.assets.size());
  for(const auto& item:r->items.poses)if(!(item.flags&2))++stats.awake_world_items;
  const auto memory=device_memory_stats(r->device->getInfo());
  stats.gpu_local_usage=memory.local_usage;stats.gpu_local_budget=memory.local_budget;
  stats.gpu_budget_available=memory.budget_available;
  stats.process_resident_bytes=memory.process_resident;stats.process_peak_bytes=memory.process_peak;
  return stats;
}
const char* open_world_renderer_status(const WorldRenderer* r) { return r?r->status.c_str():"renderer_unavailable"; }
void open_world_renderer_set_load_progress(WorldRenderer* r,WorldLoadProgressFn progress,void* user) {
  if(r) {r->load_progress=progress;r->load_progress_user=user;}
}
void world_renderer_load_stage(WorldRenderer& r,const char* stage,bool cpu_only) {
  if(r.load_progress)r.load_progress(stage,cpu_only,r.load_progress_user);
}
static bool map_glb_load(WorldRenderer& r, const std::filesystem::path& glb_path) {
  if(r.map || r.tile_session || r.scene_session) {r.status="map_already_loaded";return false;}
  if(!r.capabilities.virtual_geometry()) {
    r.status="virtual_geometry_hardware_unsupported";return false;
  }
  r.status="map_loading";
  const auto utf8=glb_path.generic_u8string();
  const auto progress=[](const char* stage,bool cpu_only,void* user) {
    world_renderer_load_stage(*static_cast<WorldRenderer*>(user),stage,cpu_only);
  };
  auto* map=create_map_renderer(r.device.get(),rhi::Format::RGBA16Float,rhi::Format::D32Float,
      reinterpret_cast<const char*>(utf8.c_str()),"octaryn-client/Shaders/Map/WorldMap.slang",true,progress,&r);
  if(!map) {r.status="map_load_failed";return false;}
  world_renderer_load_stage(r,"Preparing world residency");
  r.resident_maps.emplace_back(map,destroy_map_renderer);
  refresh_resident_texture_bytes(r);
  r.map=map;
  map->geometry=std::make_shared<virtual_geometry::WorldGeometry>();
  if(!map->geometry->initialize(r,*map)) {r.status=map->geometry->error();return false;}
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
bool open_world_renderer_map_ready(const WorldRenderer* r) {
  return r && r->map && r->map->geometry && r->map->geometry->ready();
}
bool open_world_renderer_load_scene(WorldRenderer* r,const char* catalog_path,const char* source_path) {
  if(!r || !r->queue || r->map || r->tile_session || r->scene_session || !catalog_path || !*catalog_path || !source_path || !*source_path)return false;
  std::string error;
  const auto catalog_file=std::filesystem::path(reinterpret_cast<const char8_t*>(catalog_path));
  const auto source_file=std::filesystem::path(reinterpret_cast<const char8_t*>(source_path));
  world_renderer_load_stage(*r,"Reading prepared world");
  virtual_geometry::SceneCatalog catalog;
  if(!virtual_geometry::read_scene_catalog(catalog_file,catalog,error)) {
    r->status=error;return false;
  }
  world_renderer_load_stage(*r,"Loading world roots");
  auto session=std::make_unique<SceneSession>();
  if(!session->load(*r,catalog_file,source_file)) {r->status=session->error();r->scene_memory.reset();return false;}
  r->scene_session=std::move(session);r->status="scene_loading";return true;
}
bool open_world_renderer_unload_map(WorldRenderer* r) {
  if(!r || (!r->map && !r->tile_session && !r->scene_session)) return false;
  if(!open_world_renderer_flush(r))return false;
  world_ray_release_snapshots(*r);
  r->rt_shadows.valid=false;
  r->map_reflections.valid=false;r->map_reflections.camera.invalidate();
  r->temporal.history.invalidate();r->temporal.reset=true;
  r->scene_changes.notify_column(0,0,0,0,SceneChangeKind::Removed);
  r->scene_session.reset();r->tile_session.reset();r->tile_anchor_valid=false;
  r->resident_maps.clear();r->geometry_raster.reset();
  r->resident_texture_bytes=0;
  r->scene_memory.reset();
  r->map=nullptr;
  r->status="menu";
  return true;
}
bool open_world_renderer_map_collision(const WorldRenderer* r,MapCollisionSoup* out) {
    if(!r || !r->map || !out)return false;
    const auto& model=map_model(*r->map);
    out->positions=model.vertices.empty()?nullptr:&model.vertices[0].position[0];
    out->stride_floats=sizeof(MapVertex)/sizeof(float);
    out->vertex_count=model.vertices.size();
    out->indices=model.indices.data();
    out->index_count=model.indices.size();
    return out->positions!=nullptr && out->indices!=nullptr;
}
bool open_world_renderer_flush(WorldRenderer* r) {
  return r && r->frame_queue.drain() && r->lighting_profile.drain() &&
      r->ray_diagnostics.drain(r->device) && (!r->gpu_profile || r->gpu_profile->drain()) && r->debug.errors.load()==0;
}
void open_world_renderer_release_map_geometry(WorldRenderer* r) {if(r)release_map_cpu_geometry(r->map);}
void open_world_renderer_destroy(WorldRenderer* r) { delete r; }
}
