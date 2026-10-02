#include "GraphicsHost.h"
#include "RuntimeControls.h"
#include "RuntimeSettings.h"
#include "WorldRenderer.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <exception>

namespace octaryn::client::host {
namespace {
bool valid(const octaryn_host_graphics_settings& s) {
  const auto range=[](float x,float low,float high){return std::isfinite(x) && x>=low && x<=high;};
  return s.version==1 && s.size==OCTARYN_HOST_GRAPHICS_SETTINGS_SIZE && !(s.flags&~4095u) &&
      s.window_width>=64 && s.window_width<=16384 && s.window_height>=64 && s.window_height<=16384 &&
      s.present_mode<=2 && s.upscaler_mode<=6 && (s.frame_cap_fps<=1 || (s.frame_cap_fps>=30 && s.frame_cap_fps<=240)) &&
      s.reflection_quality<=3 && s.shadow_quality<=3 && s.shadow_distance<=1024 && s.reflection_distance<=1024 &&
      s.fsr_target_fps>=30 && s.fsr_target_fps<=240 && range(s.fsr_sharpness,0,1) && range(s.fsr_render_scale,1.f/3.f,1) &&
      range(s.fsr_min_scale,1.f/3.f,1) && range(s.fsr_max_scale,s.fsr_min_scale,1);
}
bool restore_window(SDL_Window* window,bool fullscreen,int width,int height) {
  if(!SDL_SetWindowFullscreen(window,fullscreen) || (!fullscreen && !SDL_SetWindowSize(window,width,height)) ||
      !SDL_SyncWindow(window))return false;
  const bool actual_fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
  int actual_width{},actual_height{};
  return actual_fullscreen==fullscreen && (fullscreen ||
      (SDL_GetWindowSize(window,&actual_width,&actual_height) && actual_width==width && actual_height==height));
}
}
int graphics_get(const GraphicsHost* host,octaryn_host_graphics_settings* out) {
  if(!SDL_IsMainThread() || !host || !host->window || !host->controls || !host->renderer)return OCTARYN_GRAPHICS_UNAVAILABLE;
  if(!out || out->version!=1 || out->size!=OCTARYN_HOST_GRAPHICS_SETTINGS_SIZE)return OCTARYN_GRAPHICS_REJECTED;
  const auto& c=*host->controls;int width{},height{};SDL_GetWindowSize(host->window,&width,&height);
  const auto actual=rendering::open_world_renderer_stats(host->renderer);
  *out={};out->version=1;out->size=OCTARYN_HOST_GRAPHICS_SETTINGS_SIZE;
  out->flags=((SDL_GetWindowFlags(host->window)&SDL_WINDOW_FULLSCREEN)?1u:0u) |
      (c.fsr_sharpening?2u:0u)|(c.fsr_dynamic_resolution?4u:0u)|(c.ray_tracing_enabled?8u:0u)|
      (c.pbr_enabled?16u:0u)|(c.pom_enabled?32u:0u)|(c.fog_enabled?64u:0u)|(c.clouds_enabled?128u:0u)|
      (c.sky_gradient_enabled?256u:0u)|(c.stars_enabled?512u:0u)|(c.sun_enabled?1024u:0u)|(c.moon_enabled?2048u:0u);
  out->capabilities=actual.ray_tracing_available?OCTARYN_GRAPHICS_RAY_AVAILABLE:0;
  out->window_width=unsigned(width);out->window_height=unsigned(height);out->present_mode=unsigned(c.present_mode_index);
  out->frame_cap_fps=c.frame_cap_fps;out->upscaler_mode=c.upscaler_mode;
  out->reflection_quality=c.reflection_quality;out->shadow_quality=c.shadow_quality;
  out->shadow_distance=c.shadow_distance;out->reflection_distance=c.reflection_distance;out->fsr_target_fps=c.fsr_target_fps;
  out->fsr_sharpness=c.fsr_sharpness;out->fsr_render_scale=c.fsr_render_scale;
  out->fsr_min_scale=c.fsr_min_scale;out->fsr_max_scale=c.fsr_max_scale;
  out->render_width=actual.render_width;out->render_height=actual.render_height;
  out->display_width=actual.display_width;out->display_height=actual.display_height;
  return OCTARYN_GRAPHICS_APPLIED;
}
int graphics_apply(GraphicsHost* host,const octaryn_host_graphics_settings* request,unsigned persist) {
  if(!SDL_IsMainThread() || !host || !host->window || !host->controls || !host->renderer)return OCTARYN_GRAPHICS_UNAVAILABLE;
  if(!request || persist>1 || !valid(*request))return OCTARYN_GRAPHICS_REJECTED;
  if((request->flags&OCTARYN_GRAPHICS_RAY_TRACING) &&
      !rendering::open_world_renderer_stats(host->renderer).ray_tracing_available)return OCTARYN_GRAPHICS_UNAVAILABLE;
  int old_width{},old_height{};SDL_GetWindowSize(host->window,&old_width,&old_height);
  const bool old_fullscreen=(SDL_GetWindowFlags(host->window)&SDL_WINDOW_FULLSCREEN)!=0;
  const bool fullscreen=(request->flags&OCTARYN_GRAPHICS_FULLSCREEN)!=0;
  const bool display_changed=old_fullscreen!=fullscreen || (!fullscreen &&
      (unsigned(old_width)!=request->window_width || unsigned(old_height)!=request->window_height));
  if(display_changed && !restore_window(host->window,fullscreen,int(request->window_width),int(request->window_height))) {
    if(!restore_window(host->window,old_fullscreen,old_width,old_height))
      std::fprintf(stderr,"graphics_settings display_rollback_failed=1\n");
    return OCTARYN_GRAPHICS_REJECTED;
  }
  auto& c=*host->controls;
  c.fsr_sharpening=(request->flags&2)!=0;c.fsr_dynamic_resolution=(request->flags&4)!=0;
  c.ray_tracing_enabled=(request->flags&8)!=0;c.pbr_enabled=(request->flags&16)!=0;
  c.pom_enabled=(request->flags&32)!=0;c.fog_enabled=(request->flags&64)!=0;c.clouds_enabled=(request->flags&128)!=0;
  c.sky_gradient_enabled=(request->flags&256)!=0;c.stars_enabled=(request->flags&512)!=0;
  c.sun_enabled=(request->flags&1024)!=0;c.moon_enabled=(request->flags&2048)!=0;
  c.present_mode_index=int(request->present_mode);c.frame_cap_fps=uint16_t(request->frame_cap_fps);
  c.upscaler_mode=uint8_t(request->upscaler_mode);c.reflection_quality=uint8_t(request->reflection_quality);
  c.shadow_quality=uint8_t(request->shadow_quality);c.shadow_distance=uint16_t(request->shadow_distance);
  c.reflection_distance=uint16_t(request->reflection_distance);c.fsr_target_fps=uint16_t(request->fsr_target_fps);
  c.fsr_sharpness=request->fsr_sharpness;c.fsr_render_scale=request->fsr_render_scale;
  c.fsr_min_scale=request->fsr_min_scale;c.fsr_max_scale=request->fsr_max_scale;
  if(persist) {
    try {if(!runtime_settings_save(host->window,&c))return OCTARYN_GRAPHICS_APPLIED_NOT_PERSISTED;}
    catch(const std::exception&) {return OCTARYN_GRAPHICS_APPLIED_NOT_PERSISTED;}
  }
  std::printf("graphics_settings applied=1 persisted=%u next_frame=1 display_changed=%u\n",persist,unsigned(display_changed));
  return OCTARYN_GRAPHICS_APPLIED;
}
}
